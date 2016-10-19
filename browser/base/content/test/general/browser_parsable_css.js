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
let whitelist = [
  // CodeMirror is imported as-is, see bug 1004423.
  {sourceName: /codemirror\.css$/i,
   isFromDevTools: true},
  // The debugger uses cross-browser CSS.
  {sourceName: /devtools\/client\/debugger\/new\/styles.css/i,
   isFromDevTools: true},
  // PDFjs is futureproofing its pseudoselectors, and those rules are dropped.
  {sourceName: /web\/viewer\.css$/i,
   errorMessage: /Unknown pseudo-class.*(fullscreen|selection)/i,
   isFromDevTools: false},
  // Tracked in bug 1004428.
  {sourceName: /aboutaccounts\/(main|normalize)\.css$/i,
   isFromDevTools: false},
  // Highlighter CSS uses a UA-only pseudo-class, see bug 985597.
  {sourceName: /highlighters\.css$/i,
   errorMessage: /Unknown pseudo-class.*moz-native-anonymous/i,
   isFromDevTools: true},
  // Responsive Design Mode CSS uses a UA-only pseudo-class, see Bug 1241714.
  {sourceName: /responsive-ua\.css$/i,
   errorMessage: /Unknown pseudo-class.*moz-dropdown-list/i,
   isFromDevTools: true},

  {sourceName: /\b(contenteditable|EditorOverride|svg|forms|html|mathml|ua)\.css$/i,
   errorMessage: /Unknown pseudo-class.*-moz-/i,
   isFromDevTools: false},
  {sourceName: /\b(html|mathml|ua)\.css$/i,
   errorMessage: /Unknown property.*-moz-/i,
   isFromDevTools: false},
  // Reserved to UA sheets unless layout.css.overflow-clip-box.enabled flipped to true.
  {sourceName: /res\/forms\.css$/i,
   errorMessage: /Unknown property.*overflow-clip-box/i,
   isFromDevTools: false},
];

// Platform can be "linux", "macosx" or "win". If omitted, the exception applies to all platforms.
let allowedImageReferences = [
  // Bug 1302691
  {file: "chrome://devtools/skin/images/dock-bottom-minimize@2x.png",
   from: "chrome://devtools/skin/toolbox.css",
   isFromDevTools: true},
  {file: "chrome://devtools/skin/images/dock-bottom-maximize@2x.png",
   from: "chrome://devtools/skin/toolbox.css",
   isFromDevTools: true},
];

var moduleLocation = gTestPath.replace(/\/[^\/]*$/i, "/parsingTestHelpers.jsm");
var {generateURIsFromDirTree} = Cu.import(moduleLocation, {});

// Add suffix to stylesheets' URI so that we always load them here and
// have them parsed. Add a random number so that even if we run this
// test multiple times, it would be unlikely to affect each other.
const kPathSuffix = "?always-parse-css-" + Math.random();

/**
 * Check if an error should be ignored due to matching one of the whitelist
 * objects defined in whitelist
 *
 * @param aErrorObject the error to check
 * @return true if the error should be ignored, false otherwise.
 */
function ignoredError(aErrorObject) {
  for (let whitelistItem of whitelist) {
    let matches = true;
    for (let prop of ["sourceName", "errorMessage"]) {
      if (whitelistItem.hasOwnProperty(prop) &&
          !whitelistItem[prop].test(aErrorObject[prop] || "")) {
        matches = false;
        break;
      }
    }
    if (matches) {
      whitelistItem.used = true;
      return true;
    }
  }
  return false;
}

function once(target, name) {
  return new Promise((resolve, reject) => {
    let cb = () => {
      target.removeEventListener(name, cb);
      resolve();
    };
    target.addEventListener(name, cb);
  });
}

function fetchFile(uri) {
  return new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.responseType = "text";
    xhr.open("GET", uri, true);
    xhr.onreadystatechange = function() {
      if (this.readyState != this.DONE) {
        return;
      }
      try {
        resolve(this.responseText);
      } catch (ex) {
        ok(false, `Script error reading ${uri}: ${ex}`);
        resolve("");
      }
    };
    xhr.onerror = error => {
      ok(false, `XHR error reading ${uri}: ${error}`);
      resolve("");
    };
    xhr.send(null);
  });
}

var gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                 .getService(Ci.nsIChromeRegistry);
var gChromeMap = new Map();

function getBaseUriForChromeUri(chromeUri) {
  let chromeFile = chromeUri + "gobbledygooknonexistentfile.reallynothere";
  let uri = Services.io.newURI(chromeFile, null, null);
  let fileUri = gChromeReg.convertChromeURL(uri);
  return fileUri.resolve(".");
}

function parseManifest(manifestUri) {
  return fetchFile(manifestUri.spec).then(data => {
    for (let line of data.split('\n')) {
      let [type, ...argv] = line.split(/\s+/);
      let component;
      if (type == "content" || type == "skin") {
        [component] = argv;
      } else {
        // skip unrelated lines
        continue;
      }
      let chromeUri = `chrome://${component}/${type}/`;
      gChromeMap.set(getBaseUriForChromeUri(chromeUri), chromeUri);
    }
  });
}

function convertToChromeUri(fileUri) {
  let baseUri = fileUri.spec;
  let path = "";
  while (true) {
    let slashPos = baseUri.lastIndexOf("/", baseUri.length - 2);
    if (slashPos < 0) {
      info(`File not accessible from chrome protocol: ${fileUri.path}`);
      return fileUri;
    }
    path = baseUri.slice(slashPos + 1) + path;
    baseUri = baseUri.slice(0, slashPos + 1);
    if (gChromeMap.has(baseUri)) {
      let chromeBaseUri = gChromeMap.get(baseUri);
      let chromeUri = `${chromeBaseUri}${path}`;
      return Services.io.newURI(chromeUri, null, null);
    }
  }
}

function messageIsCSSError(msg) {
  // Only care about CSS errors generated by our iframe:
  if ((msg instanceof Ci.nsIScriptError) &&
      msg.category.includes("CSS") &&
      msg.sourceName.endsWith(kPathSuffix)) {
    let sourceName = msg.sourceName.slice(0, -kPathSuffix.length);
    let msgInfo = { sourceName, errorMessage: msg.errorMessage };
    // Check if this error is whitelisted in whitelist
    if (!ignoredError(msgInfo)) {
      ok(false, `Got error message for ${sourceName}: ${msg.errorMessage}`);
      return true;
    }
    info(`Ignored error for ${sourceName} because of filter.`);
  }
  return false;
}

let imageURIsToReferencesMap = new Map();

function processCSSRules(sheet) {
  for (let rule of sheet.cssRules) {
    if (rule instanceof CSSMediaRule) {
      processCSSRules(rule);
      continue;
    }
    if (!(rule instanceof CSSStyleRule))
      continue;

    // Extract urls from the css text.
    // Note: CSSStyleRule.cssText always has double quotes around URLs even
    //       when the original CSS file didn't.
    let urls = rule.cssText.match(/url\("[^"]*"\)/g);
    if (!urls)
      continue;

    for (let url of urls) {
      // Remove the url(" prefix and the ") suffix.
      url = url.replace(/url\("(.*)"\)/, "$1");
      if (url.startsWith("data:"))
        continue;

      // Make the url absolute and remove the ref.
      let baseURI = Services.io.newURI(rule.parentStyleSheet.href, null, null);
      url = Services.io.newURI(url, null, baseURI).specIgnoringRef;

      // Store the image url along with the css file referencing it.
      let baseUrl = baseURI.spec.split("?always-parse-css")[0];
      if (!imageURIsToReferencesMap.has(url)) {
        imageURIsToReferencesMap.set(url, new Set([baseUrl]));
      } else {
        imageURIsToReferencesMap.get(url).add(baseUrl);
      }
    }
  }
}

function chromeFileExists(aURI)
{
  let available = 0;
  try {
    let channel = NetUtil.newChannel({uri: aURI, loadUsingSystemPrincipal: true});
    let stream = channel.open();
    let sstream = Cc["@mozilla.org/scriptableinputstream;1"]
                    .createInstance(Ci.nsIScriptableInputStream);
    sstream.init(stream);
    available = sstream.available();
    sstream.close();
  } catch (e) {
    if (e.result != Components.results.NS_ERROR_FILE_NOT_FOUND) {
      dump("Checking " + aURI + ": " + e + "\n");
      Cu.reportError(e);
    }
  }
  return available > 0;
}

add_task(function* checkAllTheCSS() {
  let appDir = Services.dirsvc.get("GreD", Ci.nsIFile);
  // This asynchronously produces a list of URLs (sadly, mostly sync on our
  // test infrastructure because it runs against jarfiles there, and
  // our zipreader APIs are all sync)
  let uris = yield generateURIsFromDirTree(appDir, [".css", ".manifest"]);

  // Create a clean iframe to load all the files into. This needs to live at a
  // chrome URI so that it's allowed to load and parse any styles.
  let testFile = getRootDirectory(gTestPath) + "dummy_page.html";
  let windowless = Services.appShell.createWindowlessBrowser();
  let iframe = windowless.document.createElementNS("http://www.w3.org/1999/xhtml", "html:iframe");
  windowless.document.documentElement.appendChild(iframe);
  let iframeLoaded = once(iframe, 'load');
  iframe.contentWindow.location = testFile;
  yield iframeLoaded;
  let doc = iframe.contentWindow.document;

  // Parse and remove all manifests from the list.
  // NOTE that this must be done before filtering out devtools paths
  // so that all chrome paths can be recorded.
  let manifestPromises = [];
  uris = uris.filter(uri => {
    if (uri.path.endsWith(".manifest")) {
      manifestPromises.push(parseManifest(uri));
      return false;
    }
    return true;
  });
  // Wait for all manifest to be parsed
  yield Promise.all(manifestPromises);

  // We build a list of promises that get resolved when their respective
  // files have loaded and produced no errors.
  let allPromises = [];

  // filter out either the devtools paths or the non-devtools paths:
  let isDevtools = SimpleTest.harnessParameters.subsuite == "devtools";
  let devtoolsPathBits = ["webide", "devtools"];
  uris = uris.filter(uri => isDevtools == devtoolsPathBits.some(path => uri.spec.includes(path)));

  for (let uri of uris) {
    let linkEl = doc.createElement("link");
    linkEl.setAttribute("rel", "stylesheet");
    let promiseForThisSpec = Promise.defer();
    let onLoad = (e) => {
      processCSSRules(linkEl.sheet);
      promiseForThisSpec.resolve();
      linkEl.removeEventListener("load", onLoad);
      linkEl.removeEventListener("error", onError);
    };
    let onError = (e) => {
      ok(false, "Loading " + linkEl.getAttribute("href") + " threw an error!");
      promiseForThisSpec.resolve();
      linkEl.removeEventListener("load", onLoad);
      linkEl.removeEventListener("error", onError);
    };
    linkEl.addEventListener("load", onLoad);
    linkEl.addEventListener("error", onError);
    linkEl.setAttribute("type", "text/css");
    let chromeUri = convertToChromeUri(uri);
    linkEl.setAttribute("href", chromeUri.spec + kPathSuffix);
    allPromises.push(promiseForThisSpec.promise);
    doc.head.appendChild(linkEl);
  }

  // Wait for all the files to have actually loaded:
  yield Promise.all(allPromises);

  // Check if all the files referenced from CSS actually exist.
  for (let [image, references] of imageURIsToReferencesMap) {
    if (!chromeFileExists(image)) {
      for (let ref of references) {
        let ignored = false;
        for (let item of allowedImageReferences) {
          if (image.endsWith(item.file) && ref.endsWith(item.from) &&
              isDevtools == item.isFromDevTools &&
              (!item.platforms || item.platforms.includes(AppConstants.platform))) {
            item.used = true;
            ignored = true;
            break;
          }
        }
        if (!ignored)
          ok(false, "missing " + image + " referenced from " + ref);
      }
    }
  }

  let messages = Services.console.getMessageArray();
  // Count errors (the test output will list actual issues for us, as well
  // as the ok(false) in messageIsCSSError.
  let errors = messages.filter(messageIsCSSError);
  is(errors.length, 0, "All the styles (" + allPromises.length + ") loaded without errors.");

  // Confirm that all whitelist rules have been used.
  for (let item of whitelist) {
    if (!item.used && isDevtools == item.isFromDevTools) {
      ok(false, "Unused whitelist item. " +
                (item.sourceName ? " sourceName: " + item.sourceName : "") +
                (item.errorMessage ? " errorMessage: " + item.errorMessage : ""));
    }
  }

  // Confirm that all file whitelist rules have been used.
  for (let item of allowedImageReferences) {
    if (!item.used && isDevtools == item.isFromDevTools &&
        (!item.platforms || item.platforms.includes(AppConstants.platform))) {
      ok(false, "Unused file whitelist item. " +
                " file: " + item.file +
                " from: " + item.from);
    }
  }

  // Clean up to avoid leaks:
  iframe.remove();
  doc.head.innerHTML = '';
  doc = null;
  iframe = null;
  windowless.close();
  windowless = null;
  imageURIsToReferencesMap = null;
});
