/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Slow on asan builds.
requestLongerTimeout(5);

var isDevtools = SimpleTest.harnessParameters.subsuite == "devtools";

var gExceptionPaths = ["chrome://browser/content/defaultthemes/",
                       "chrome://browser/locale/searchplugins/"];

var whitelist = new Set([
  // browser/extensions/pdfjs/content/PdfStreamConverter.jsm
  {file: "chrome://pdf.js/locale/chrome.properties"},
  {file: "chrome://pdf.js/locale/viewer.properties"},

  // security/manager/pki/resources/content/device_manager.js
  {file: "chrome://pippki/content/load_device.xul"},

  // browser/modules/ReaderParent.jsm
  {file: "chrome://browser/skin/reader-tour.png"},
  {file: "chrome://browser/skin/reader-tour@2x.png"},

  // Used by setting this url as a pref in about:config
  {file: "chrome://browser/content/newtab/alternativeDefaultSites.json"},

  // Add-on compat
  {file: "chrome://browser/skin/devtools/common.css"},
  {file: "chrome://global/content/XPCNativeWrapper.js"},
  {file: "chrome://global/locale/brand.dtd"},

  // The l10n build system can't package string files only for some platforms.
  // See bug 1339424 for why this is hard to fix.
  {file: "chrome://global/locale/fallbackMenubar.properties",
   platforms: ["linux", "win"]},
  {file: "chrome://global/locale/printPageSetup.dtd", platforms: ["macosx"]},
  {file: "chrome://global/locale/printPreviewProgress.dtd",
   platforms: ["macosx"]},
  {file: "chrome://global/locale/printProgress.dtd", platforms: ["macosx"]},
  {file: "chrome://global/locale/printdialog.dtd",
   platforms: ["macosx", "win"]},
  {file: "chrome://global/locale/printjoboptions.dtd",
   platforms: ["macosx", "win"]},

  // services/cloudsync/CloudSyncLocal.jsm
  {file: "chrome://weave/locale/errors.properties"},

  // devtools/client/inspector/bin/dev-server.js
  {file: "chrome://devtools/content/inspector/markup/markup.xhtml",
   isFromDevTools: true},

  // Starting from here, files in the whitelist are bugs that need fixing.
  // Bug 1339420
  {file: "chrome://branding/content/icon128.png"},
  // Bug 1339424 (wontfix?)
  {file: "chrome://browser/locale/taskbar.properties",
   platforms: ["linux", "macosx"]},
  // Bug 1320156
  {file: "chrome://browser/skin/Privacy-16.png", platforms: ["linux"]},
  // Bug 1343584
  {file: "chrome://browser/skin/click-to-play-warning-stripes.png"},
  // Bug 1343824
  {file: "chrome://browser/skin/customizableui/customize-illustration-rtl@2x.png",
   platforms: ["linux", "win"]},
  {file: "chrome://browser/skin/customizableui/customize-illustration@2x.png",
   platforms: ["linux", "win"]},
  {file: "chrome://browser/skin/customizableui/info-icon-customizeTip@2x.png",
   platforms: ["linux", "win"]},
  {file: "chrome://browser/skin/customizableui/panelarrow-customizeTip@2x.png",
   platforms: ["linux", "win"]},
  // Bug 1320058
  {file: "chrome://browser/skin/preferences/saveFile.png", platforms: ["win"]},
  // Bug 1348369
  {file: "chrome://formautofill/content/editProfile.xhtml"},
  // Bug 1316187
  {file: "chrome://global/content/customizeToolbar.xul"},
  // Bug 1343837
  {file: "chrome://global/content/findUtils.js"},
  // Bug 1343843
  {file: "chrome://global/content/url-classifier/unittests.xul"},
  // Bug 1343839
  {file: "chrome://global/locale/headsUpDisplay.properties"},
  // Bug 1348358
  {file: "chrome://global/skin/arrow.css"},
  {file: "chrome://global/skin/arrow/arrow-dn-sharp.gif",
   platforms: ["linux", "win"]},
  {file: "chrome://global/skin/arrow/arrow-down.png",
   platforms: ["linux", "win"]},
  {file: "chrome://global/skin/arrow/arrow-lft-sharp-end.gif"},
  {file: "chrome://global/skin/arrow/arrow-lft-sharp.gif",
   platforms: ["linux", "win"]},
  {file: "chrome://global/skin/arrow/arrow-rit-sharp-end.gif"},
  {file: "chrome://global/skin/arrow/arrow-rit-sharp.gif",
   platforms: ["linux", "win"]},
  {file: "chrome://global/skin/arrow/arrow-up-sharp.gif",
   platforms: ["linux", "win"]},
  {file: "chrome://global/skin/arrow/panelarrow-horizontal.svg",
   platforms: ["linux"]},
  {file: "chrome://global/skin/arrow/panelarrow-vertical.svg",
   platforms: ["linux"]},
  // Bug 1348359
  {file: "chrome://global/skin/dirListing/folder.png", platforms: ["linux"]},
  {file: "chrome://global/skin/dirListing/local.png", platforms: ["linux", "win"]},
  {file: "chrome://global/skin/dirListing/remote.png"},
  {file: "chrome://global/skin/dirListing/up.png", platforms: ["linux"]},
  // Bug 1348362
  {file: "chrome://global/skin/icons/Close.gif", platforms: ["win"]},
  {file: "chrome://global/skin/icons/Error.png", platforms: ["linux", "macosx"]},
  {file: "chrome://global/skin/icons/Landscape.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/Minimize.gif", platforms: ["win"]},
  {file: "chrome://global/skin/icons/Portrait.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/Print-preview.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/Question.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/Restore.gif", platforms: ["win"]},
  {file: "chrome://global/skin/icons/Search-close.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/Search-glass.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/Warning.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/checkbox.png", platforms: ["macosx"]},
  {file: "chrome://global/skin/icons/checkbox@2x.png", platforms: ["macosx"]},
  {file: "chrome://global/skin/icons/close-inverted.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/close-inverted@2x.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/close.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/close@2x.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/collapse.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/error-64.png", platforms: ["linux", "win"]},
  {file: "chrome://global/skin/icons/error-large.png", platforms: ["macosx"]},
  {file: "chrome://global/skin/icons/expand.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/folder-item.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/question-large.png", platforms: ["macosx"]},
  {file: "chrome://global/skin/icons/warning-32.png", platforms: ["macosx"]},
  {file: "chrome://global/skin/icons/warning-64.png", platforms: ["linux", "win"]},
  {file: "chrome://global/skin/icons/warning-large.png", platforms: ["linux"]},
  {file: "chrome://global/skin/icons/windowControls.png", platforms: ["linux"]},
  // Bug 1348521
  {file: "chrome://global/skin/linkTree.css"},
  // Bug 1348522
  {file: "chrome://global/skin/media/clicktoplay-bgtexture.png"},
  {file: "chrome://global/skin/media/videoClickToPlayButton.svg"},
  // Bug 1348524
  {file: "chrome://global/skin/notification/close.png", platforms: ["macosx"]},
  // Bug 1348525
  {file: "chrome://global/skin/splitter/grip-bottom.gif", platforms: ["linux"]},
  {file: "chrome://global/skin/splitter/grip-left.gif", platforms: ["linux"]},
  {file: "chrome://global/skin/splitter/grip-right.gif", platforms: ["linux"]},
  {file: "chrome://global/skin/splitter/grip-top.gif", platforms: ["linux"]},
  // Bug 1348526
  {file: "chrome://global/skin/tree/sort-asc-classic.png", platforms: ["linux"]},
  {file: "chrome://global/skin/tree/sort-asc.png", platforms: ["linux"]},
  {file: "chrome://global/skin/tree/sort-dsc-classic.png", platforms: ["linux"]},
  {file: "chrome://global/skin/tree/sort-dsc.png", platforms: ["linux"]},
  // Bug 1344267
  {file: "chrome://marionette/content/test_anonymous_content.xul"},
  {file: "chrome://marionette/content/test_dialog.properties"},
  {file: "chrome://marionette/content/test_dialog.xul"},
  // Bug 1348532
  {file: "chrome://mozapps/content/extensions/list.xul"},
  // Bug 1348533
  {file: "chrome://mozapps/skin/downloads/buttons.png", platforms: ["macosx"]},
  {file: "chrome://mozapps/skin/downloads/downloadButtons.png", platforms: ["linux", "win"]},
  // Bug 1348555
  {file: "chrome://mozapps/skin/extensions/dictionaryGeneric-16.png"},
  {file: "chrome://mozapps/skin/extensions/search.png", platforms: ["macosx"]},
  {file: "chrome://mozapps/skin/extensions/themeGeneric-16.png"},
  // Bug 1348556
  {file: "chrome://mozapps/skin/plugins/pluginBlocked.png"},
  // Bug 1348558
  {file: "chrome://mozapps/skin/update/downloadButtons.png",
   platforms: ["linux"]},
  // Bug 1348559
  {file: "chrome://pippki/content/resetpassword.xul"},

].filter(item =>
  ("isFromDevTools" in item) == isDevtools &&
  (!item.platforms || item.platforms.includes(AppConstants.platform))
).map(item => item.file));

const ignorableWhitelist = new Set([
  // chrome://xslt-qa/ isn't referenced, but isn't included in packaged builds,
  // so it's fine to just ignore it and ignore if the exceptions are unused.
  "chrome://xslt-qa/content/buster/result-view.xul",
  "chrome://xslt-qa/content/xslt-qa-overlay.xul",
  // The communicator.css file is kept for add-on backward compat, but it is
  // referenced by something in xslt-qa, so the exception won't be used when
  // running the test on a local non-packaged build.
  "chrome://communicator/skin/communicator.css",

  // These 2 files are unreferenced only when building without the crash
  // reporter (eg. Linux x64 asan builds on treeherder)
  "chrome://global/locale/crashes.dtd",
  "chrome://global/locale/crashes.properties",
]);
for (let entry of ignorableWhitelist)
  whitelist.add(entry);

const gInterestingCategories = new Set([
  "agent-style-sheets", "webextension-scripts",
  "webextension-schemas", "webextension-scripts-addon",
  "webextension-scripts-content", "webextension-scripts-devtools"
]);

var gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                 .getService(Ci.nsIChromeRegistry);
var gChromeMap = new Map();
var gOverrideMap = new Map();
var gReferencesFromCode = new Set();

function getBaseUriForChromeUri(chromeUri) {
  let chromeFile = chromeUri + "gobbledygooknonexistentfile.reallynothere";
  let uri = Services.io.newURI(chromeFile);
  let fileUri = gChromeReg.convertChromeURL(uri);
  return fileUri.resolve(".");
}

function parseManifest(manifestUri) {
  return fetchFile(manifestUri.spec).then(data => {
    for (let line of data.split("\n")) {
      let [type, ...argv] = line.split(/\s+/);
      if (type == "content" || type == "skin" || type == "locale") {
        let chromeUri = `chrome://${argv[0]}/${type}/`;
        gChromeMap.set(getBaseUriForChromeUri(chromeUri), chromeUri);
      } else if (type == "override" || type == "overlay") {
        // Overlays aren't really overrides, but behave the same in
        // that the overlay is only referenced if the original xul
        // file is referenced somewhere.
        let os = "os=" + Services.appinfo.OS;
        if (!argv.some(s => s.startsWith("os=") && s != os)) {
          gOverrideMap.set(Services.io.newURI(argv[1]).specIgnoringRef,
                           Services.io.newURI(argv[0]).specIgnoringRef);
        }
      } else if (type == "category" && gInterestingCategories.has(argv[0])) {
        gReferencesFromCode.add(argv[2]);
      }
    }
  });
}

function parseCSSFile(fileUri) {
  return fetchFile(fileUri.spec).then(data => {
    for (let line of data.split("\n")) {
      let urls = line.match(/url\([^()]+\)/g);
      if (!urls) {
        // @import rules can take a string instead of a url.
        let importMatch = line.match(/@import ['"]?([^'"]*)['"]?/);
        if (importMatch && importMatch[1]) {
          let url = Services.io.newURI(importMatch[1], null, fileUri).spec;
          gReferencesFromCode.add(convertToChromeUri(url));
        }
        continue;
      }

      for (let url of urls) {
        // Remove the url(" prefix and the ") suffix.
        url = url.replace(/url\(([^)]*)\)/, "$1")
                 .replace(/^"(.*)"$/, "$1")
                 .replace(/^'(.*)'$/, "$1");
        if (url.startsWith("data:"))
          continue;

        try {
          url = Services.io.newURI(url, null, fileUri).specIgnoringRef;
          gReferencesFromCode.add(convertToChromeUri(url));
        } catch (e) {
          ok(false, "unexpected error while resolving this URI: " + url);
        }
      }
    }
  });
}

function parseCodeFile(fileUri) {
  return fetchFile(fileUri.spec).then(data => {
    for (let line of data.split("\n")) {
      let urls =
        line.match(/["']chrome:\/\/[a-zA-Z0-9 -]+\/(content|skin|locale)\/[^"' ]*["']/g);
      if (!urls) {
        // If there's no absolute chrome URL, look for relative ones in
        // src and href attributes.
        let match = line.match("(?:src|href)=[\"']([^$&\"']+)");
        if (match && match[1]) {
          let url = Services.io.newURI(match[1], null, fileUri).spec;
          gReferencesFromCode.add(convertToChromeUri(url));
        }

        if (isDevtools) {
          // Handle usage of devtools' LocalizationHelper object
          match = line.match('"devtools/client/locales/([^/.]+).properties"');
          if (match && match[1]) {
            gReferencesFromCode.add("chrome://devtools/locale/" +
                                    match[1] + ".properties");
          }

          match = line.match('"devtools/shared/locales/([^/.]+).properties"');
          if (match && match[1]) {
            gReferencesFromCode.add("chrome://devtools-shared/locale/" +
                                    match[1] + ".properties");
          }
        }
        continue;
      }

      for (let url of urls) {
        // Remove quotes.
        url = url.slice(1, -1);
        // Remove ? or \ trailing characters.
        if (url.endsWith("?") || url.endsWith("\\"))
          url = url.slice(0, -1);

        // Make urls like chrome://browser/skin/ point to an actual file,
        // and remove the ref if any.
        url = Services.io.newURI(url).specIgnoringRef;

        gReferencesFromCode.add(url);
      }
    }
  });
}

function convertToChromeUri(fileUri) {
  let baseUri = fileUri;
  let path = "";
  while (true) {
    let slashPos = baseUri.lastIndexOf("/", baseUri.length - 2);
    if (slashPos <= 0) {
      // File not accessible from chrome protocol,
      // TODO: bug 1349005 handle resource:// urls.
      return fileUri;
    }
    path = baseUri.slice(slashPos + 1) + path;
    baseUri = baseUri.slice(0, slashPos + 1);
    if (gChromeMap.has(baseUri)) {
      let chromeBaseUri = gChromeMap.get(baseUri);
      return `${chromeBaseUri}${path}`;
    }
  }
}

function chromeFileExists(aURI) {
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

function findChromeUrlsFromArray(array) {
  const prefix = "chrome://";
  // Find the 'c' character...
  for (let index = 0;
       (index = array.indexOf(prefix.charCodeAt(0), index)) != -1;
       ++index) {
    // Then ensure we actually have the whole chrome:// prefix.
    let found = true;
    for (let i = 1; i < prefix.length; ++i) {
      if (array[index + i] != prefix.charCodeAt(i)) {
        found = false;
        break;
      }
    }
    if (!found)
      continue;

    // C strings are null terminated, but " also terminates urls
    // (nsIndexedToHTML.cpp contains an HTML fragment with several chrome urls)
    // Let's also terminate the string on the # character to skip references.
    let end = Math.min(array.indexOf(0, index),
                       array.indexOf('"'.charCodeAt(0), index),
                       array.indexOf("#".charCodeAt(0), index));
    let string = "";
    for ( ; index < end; ++index)
      string += String.fromCharCode(array[index]);

    // Only keep strings that look like real chrome urls.
    if (/chrome:\/\/[a-zA-Z09 -]+\/(content|skin|locale)\//.test(string))
      gReferencesFromCode.add(string);
  }
}

add_task(function* checkAllTheFiles() {
  let libxulPath = OS.Constants.Path.libxul;
  if (AppConstants.platform != "macosx")
    libxulPath = OS.Constants.Path.libDir + "/" + libxulPath;
  let libxul = yield OS.File.read(libxulPath);
  findChromeUrlsFromArray(libxul);
  // Handle NS_LITERAL_STRING.
  findChromeUrlsFromArray(new Uint16Array(libxul.buffer));

  const kCodeExtensions = [".xul", ".xml", ".xsl", ".js", ".jsm", ".html", ".xhtml"];

  let appDir = Services.dirsvc.get("GreD", Ci.nsIFile);
  // This asynchronously produces a list of URLs (sadly, mostly sync on our
  // test infrastructure because it runs against jarfiles there, and
  // our zipreader APIs are all sync)
  let uris = yield generateURIsFromDirTree(appDir, [".css", ".manifest", ".json", ".jpg", ".png", ".gif", ".svg",  ".dtd", ".properties"].concat(kCodeExtensions));

  // Parse and remove all manifests from the list.
  // NOTE that this must be done before filtering out devtools paths
  // so that all chrome paths can be recorded.
  let manifestPromises = [];
  uris = uris.filter(uri => {
    let path = uri.path;
    if (path.endsWith(".manifest")) {
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

  for (let uri of uris) {
    let path = uri.path;
    if (path.endsWith(".css"))
      allPromises.push(parseCSSFile(uri));
    else if (kCodeExtensions.some(ext => path.endsWith(ext)))
      allPromises.push(parseCodeFile(uri));
  }

  // Wait for all the files to have actually loaded:
  yield Promise.all(allPromises);

  // Keep only chrome:// files, and filter out either the devtools paths or
  // the non-devtools paths:
  let devtoolsPrefixes = ["chrome://webide/", "chrome://devtools"];
  let chromeFiles =
    uris.map(uri => convertToChromeUri(uri.spec))
        .filter(u => u.startsWith("chrome://"))
        .filter(u => isDevtools == devtoolsPrefixes.some(prefix => u.startsWith(prefix)));

  let isUnreferenced =
    file => !gReferencesFromCode.has(file) &&
            !gExceptionPaths.some(e => file.startsWith(e)) &&
            (!gOverrideMap.has(file) || isUnreferenced(gOverrideMap.get(file)));

  let notWhitelisted = file => {
    if (!whitelist.has(file))
      return true;
    whitelist.delete(file);
    return false;
  };

  let unreferencedFiles =
    chromeFiles.filter(isUnreferenced).filter(notWhitelisted).sort();

  is(unreferencedFiles.length, 0, "there should be no unreferenced files");
  for (let file of unreferencedFiles)
    ok(false, "unreferenced chrome file: " + file);

  for (let file of whitelist) {
    if (ignorableWhitelist.has(file))
      info("ignored unused whitelist entry: " + file);
    else
      ok(false, "unused whitelist entry: " + file);
  }

  for (let file of gReferencesFromCode) {
    if (isDevtools != devtoolsPrefixes.some(prefix => file.startsWith(prefix)))
      continue;

    if (file.startsWith("chrome://") && !chromeFileExists(file)) {
      // Ignore chrome prefixes that have been automatically expanded.
      let pathParts =
        file.match("chrome://([^/]+)/content/([^/.]+)\.xul") ||
        file.match("chrome://([^/]+)/skin/([^/.]+)\.css");
      if (!pathParts || pathParts[1] != pathParts[2]) {
        // TODO: bug 1349010 - add a whitelist and make this reliable enough
        // that we could make the test fail when this catches something new.
        info("missing file with code reference: " + file);
      }
    }
  }
});
