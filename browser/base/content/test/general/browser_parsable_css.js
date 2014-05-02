/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This list allows pre-existing or 'unfixable' CSS issues to remain, while we
 * detect newly occurring issues in shipping CSS. It is a list of objects
 * specifying conditions under which an error should be ignored.
 *
 * Every property of the objects in it needs to consist of a regular expression
 * matching the offending error. If an object has multiple regex criteria, they
 * ALL need to match an error in order for that error not to cause a test
 * failure. */
const kWhitelist = [
  {sourceName: /cleopatra.*(tree|ui)\.css/i}, /* Cleopatra is imported as-is, see bug 1004421 */
  {sourceName: /codemirror\.css/i}, /* CodeMirror is imported as-is, see bug 1004423 */
  {sourceName: /web\/viewer\.css/i, errorMessage: /Unknown pseudo-class.*(fullscreen|selection)/i }, /* PDFjs is futureproofing its pseudoselectors, and those rules are dropped. */
  {sourceName: /aboutaccounts\/(main|normalize)\.css/i}, /* Tracked in bug 1004428 */
];

/**
 * Check if an error should be ignored due to matching one of the whitelist
 * objects defined in kWhitelist
 *
 * @param aErrorObject the error to check
 * @return true if the error should be ignored, false otherwise.
 */
function ignoredError(aErrorObject) {
  for (let whitelistItem of kWhitelist) {
    let matches = true;
    for (let prop in whitelistItem) {
      if (!whitelistItem[prop].test(aErrorObject[prop] || "")) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return true;
    }
  }
  return false;
}


/**
 * Returns a promise that is resolved with a list of CSS files to check,
 * represented by their nsIURI objects.
 *
 * @param appDir the application directory to scan for CSS files (nsIFile)
 */
function generateURIsFromDirTree(appDir) {
  let rv = [];
  let dirQueue = [appDir.path];
  return Task.spawn(function*() {
    while (dirQueue.length) {
      let nextDir = dirQueue.shift();
      let {subdirs, cssfiles} = yield iterateOverPath(nextDir);
      dirQueue = dirQueue.concat(subdirs);
      rv = rv.concat(cssfiles);
    }
    return rv;
  });
}

/* Shorthand constructor to construct an nsI(Local)File */
let LocalFile = Components.Constructor("@mozilla.org/file/local;1", Ci.nsIFile, "initWithPath");

/**
 * Uses OS.File.DirectoryIterator to asynchronously iterate over a directory.
 * It returns a promise that is resolved with an object with two properties:
 *  - cssfiles: an array of nsIURIs corresponding to CSS that needs checking
 *  - subdirs: an array of paths for subdirectories we need to recurse into
 *             (handled by generateURIsFromDirTree above)
 *
 * @param path the path to check (string)
 */
function iterateOverPath(path) {
  let iterator = new OS.File.DirectoryIterator(path);
  let parentDir = new LocalFile(path);
  let subdirs = [];
  let cssfiles = [];
  // Iterate through the directory
  let promise = iterator.forEach(
    function onEntry(entry) {
      if (entry.isDir) {
        let subdir = parentDir.clone();
        subdir.append(entry.name);
        subdirs.push(subdir.path);
      } else if (entry.name.endsWith(".css")) {
        let file = parentDir.clone();
        file.append(entry.name);
        let uriSpec = getURLForFile(file);
        cssfiles.push(Services.io.newURI(uriSpec, null, null));
      } else if (entry.name.endsWith(".ja")) {
        let file = parentDir.clone();
        file.append(entry.name);
        let subentries = [uri for (uri of generateEntriesFromJarFile(file))];
        cssfiles = cssfiles.concat(subentries);
      }
    }
  );

  let outerPromise = Promise.defer();
  promise.then(function() {
    outerPromise.resolve({cssfiles: cssfiles, subdirs: subdirs});
    iterator.close();
  }, function(e) {
    outerPromise.reject(e);
    iterator.close();
  });
  return outerPromise.promise;
}

/* Helper function to generate a URI spec (NB: not an nsIURI yet!)
 * given an nsIFile object */
function getURLForFile(file) {
  let fileHandler = Services.io.getProtocolHandler("file");
  fileHandler = fileHandler.QueryInterface(Ci.nsIFileProtocolHandler);
  return fileHandler.getURLSpecFromFile(file);
}

/**
 * A generator that generates nsIURIs for CSS files found in jar files
 * like omni.ja.
 *
 * @param jarFile an nsIFile object for the jar file that needs checking.
 */
function* generateEntriesFromJarFile(jarFile) {
  const ZipReader = new Components.Constructor("@mozilla.org/libjar/zip-reader;1", "nsIZipReader", "open");
  let zr = new ZipReader(jarFile);
  let entryEnumerator = zr.findEntries("*.css$");

  const kURIStart = getURLForFile(jarFile);
  while (entryEnumerator.hasMore()) {
    let entry = entryEnumerator.getNext();
    let entryURISpec = "jar:" + kURIStart + "!/" + entry;
    yield Services.io.newURI(entryURISpec, null, null);
  }
  zr.close();
}

/**
 * The actual test.
 */
add_task(function checkAllTheCSS() {
  let appDir = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
  // This asynchronously produces a list of URLs (sadly, mostly sync on our
  // test infrastructure because it runs against jarfiles there, and
  // our zipreader APIs are all sync)
  let uris = yield generateURIsFromDirTree(appDir);

  // Create a clean iframe to load all the files into:
  let hiddenWin = Services.appShell.hiddenDOMWindow;
  let iframe = hiddenWin.document.createElementNS("http://www.w3.org/1999/xhtml", "html:iframe");
  hiddenWin.document.documentElement.appendChild(iframe);
  let doc = iframe.contentWindow.document;


  // Listen for errors caused by the CSS:
  let errorListener = {
    observe: function(aMessage) {
      if (!aMessage || !(aMessage instanceof Ci.nsIScriptError)) {
        return;
      }
      // Only care about CSS errors generated by our iframe:
      if (aMessage.category.contains("CSS") && aMessage.innerWindowID === 0 && aMessage.outerWindowID === 0) {
        // Check if this error is whitelisted in kWhitelist
        if (!ignoredError(aMessage)) {
          ok(false, "Got error message for " + aMessage.sourceName + ": " + aMessage.errorMessage);
          errors++;
        } else {
          info("Ignored error for " + aMessage.sourceName + " because of filter.");
        }
      }
    }
  };

  // We build a list of promises that get resolved when their respective
  // files have loaded and produced no errors.
  let allPromises = [];
  let errors = 0;
  // Register the error listener to keep track of errors.
  Services.console.registerListener(errorListener);
  for (let uri of uris) {
    let linkEl = doc.createElement("link");
    linkEl.setAttribute("rel", "stylesheet");
    let promiseForThisSpec = Promise.defer();
    let onLoad = (e) => {
      promiseForThisSpec.resolve();
      linkEl.removeEventListener("load", onLoad);
      linkEl.removeEventListener("error", onError);
    };
    let onError = (e) => {
      promiseForThisSpec.reject({error: e, href: linkEl.getAttribute("href")});
      linkEl.removeEventListener("load", onLoad);
      linkEl.removeEventListener("error", onError);
    };
    linkEl.addEventListener("load", onLoad);
    linkEl.addEventListener("error", onError);
    linkEl.setAttribute("type", "text/css");
    linkEl.setAttribute("href", uri.spec);
    allPromises.push(promiseForThisSpec.promise);
    doc.head.appendChild(linkEl);
  }

  // Wait for all the files to have actually loaded:
  yield Promise.all(allPromises);
  // Count errors (the test output will list actual issues for us, as well
  // as the ok(false) in the error listener)
  is(errors, 0, "All the styles (" + allPromises.length + ") loaded without errors.");

  // Clean up to avoid leaks:
  Services.console.unregisterListener(errorListener);
  iframe.remove();
  doc.head.innerHTML = '';
  doc = null;
  iframe = null;
});
