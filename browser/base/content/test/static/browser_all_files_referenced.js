/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Note to run this test similar to try server, you need to run:
// ./mach package
// ./mach mochitest --appname dist <path to test>

// Slow on asan builds.
requestLongerTimeout(5);

var isDevtools = SimpleTest.harnessParameters.subsuite == "devtools";

var gExceptionPaths = [
  "chrome://browser/content/defaultthemes/",
  "chrome://browser/locale/searchplugins/",
  "resource://app/defaults/blocklists/",
  "resource://app/defaults/pinning/",
  "resource://app/defaults/preferences/",
  "resource://gre/modules/commonjs/",
  "resource://gre/defaults/pref/",
  "resource://shield-recipe-client/node_modules/jexl/lib/",

  // https://github.com/mozilla/normandy/issues/577
  "resource://shield-recipe-client/test/",

  // browser/extensions/pdfjs/content/build/pdf.js#1999
  "resource://pdf.js/web/images/",
];

// These are not part of the omni.ja file, so we find them only when running
// the test on a non-packaged build.
if (AppConstants.platform == "macosx")
  gExceptionPaths.push("resource://gre/res/cursors/");

var whitelist = [
  // browser/extensions/pdfjs/content/PdfStreamConverter.jsm
  {file: "chrome://pdf.js/locale/chrome.properties"},
  {file: "chrome://pdf.js/locale/viewer.properties"},

  // security/manager/pki/resources/content/device_manager.js
  {file: "chrome://pippki/content/load_device.xul"},

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

  // devtools/client/inspector/bin/dev-server.js
  {file: "chrome://devtools/content/inspector/markup/markup.xhtml",
   isFromDevTools: true},

  // Kept for add-on compatibility, should be removed in bug 851471.
  {file: "chrome://mozapps/skin/downloads/downloadIcon.png"},

  // extensions/pref/autoconfig/src/nsReadConfig.cpp
  {file: "resource://gre/defaults/autoconfig/prefcalls.js"},

  // modules/libpref/Preferences.cpp
  {file: "resource://gre/greprefs.js"},

  // browser/extensions/pdfjs/content/web/viewer.js
  {file: "resource://pdf.js/build/pdf.worker.js"},

  // Add-on API introduced in bug 1118285
  {file: "resource://app/modules/NewTabURL.jsm"},

  // browser/components/newtab bug 1355166
  {file: "resource://app/modules/NewTabSearchProvider.jsm"},
  {file: "resource://app/modules/NewTabWebChannel.jsm"},

  // Activity Stream currently needs this file in all channels except Nightly
  {file: "resource://app/modules/PreviewProvider.jsm", skipNightly: true},

  // layout/mathml/nsMathMLChar.cpp
  {file: "resource://gre/res/fonts/mathfontSTIXGeneral.properties"},
  {file: "resource://gre/res/fonts/mathfontUnicode.properties"},

  // toolkit/components/places/ColorAnalyzer_worker.js
  {file: "resource://gre/modules/ClusterLib.js"},
  {file: "resource://gre/modules/ColorConversion.js"},

  // The l10n build system can't package string files only for some platforms.
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/mac/accessible.properties",
   platforms: ["linux", "win"]},
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/mac/intl.properties",
   platforms: ["linux", "win"]},
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/mac/platformKeys.properties",
   platforms: ["linux", "win"]},
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/unix/accessible.properties",
   platforms: ["macosx", "win"]},
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/unix/intl.properties",
   platforms: ["macosx", "win"]},
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/unix/platformKeys.properties",
   platforms: ["macosx", "win"]},
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/win/accessible.properties",
   platforms: ["linux", "macosx"]},
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/win/intl.properties",
   platforms: ["linux", "macosx"]},
  {file: "resource://gre/chrome/en-US/locale/en-US/global-platform/win/platformKeys.properties",
   platforms: ["linux", "macosx"]},

  // browser/extensions/pdfjs/content/web/viewer.js#7450
  {file: "resource://pdf.js/web/debugger.js"},

  // Starting from here, files in the whitelist are bugs that need fixing.
  // Bug 1339420
  {file: "chrome://branding/content/icon128.png"},
  // Bug 1339424 (wontfix?)
  {file: "chrome://browser/locale/taskbar.properties",
   platforms: ["linux", "macosx"]},
  // Bug 1316187
  {file: "chrome://global/content/customizeToolbar.xul"},
  // Bug 1343837
  {file: "chrome://global/content/findUtils.js"},
  // Bug 1343843
  {file: "chrome://global/content/url-classifier/unittests.xul"},
  // Bug 1343839
  {file: "chrome://global/locale/headsUpDisplay.properties"},
  // Bug 1348362
  {file: "chrome://global/skin/icons/warning-64.png", platforms: ["linux", "win"]},
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
  // Bug 1348533
  {file: "chrome://mozapps/skin/downloads/buttons.png", platforms: ["macosx"]},
  {file: "chrome://mozapps/skin/downloads/downloadButtons.png", platforms: ["linux", "win"]},
  // Bug 1348556
  {file: "chrome://mozapps/skin/plugins/pluginBlocked.png"},
  // Bug 1348558
  {file: "chrome://mozapps/skin/update/downloadButtons.png",
   platforms: ["linux"]},
  // Bug 1348559
  {file: "chrome://pippki/content/resetpassword.xul"},
  // Bug 1351078
  {file: "resource://gre/modules/Battery.jsm"},
  // Bug 1351070
  {file: "resource://gre/modules/ContentPrefInstance.jsm"},
  // Bug 1351079
  {file: "resource://gre/modules/ISO8601DateUtils.jsm"},
  // Bug 1337345
  {file: "resource://gre/modules/Manifest.jsm"},
  // Bug 1351097
  {file: "resource://gre/modules/accessibility/AccessFu.jsm"},
  // Bug 1351637
  {file: "resource://gre/modules/sdk/bootstrap.js"},

];

if (!AppConstants.MOZ_PHOTON_THEME) {
  whitelist.push(
    // Bug 1343824
    {file: "chrome://browser/skin/customizableui/customize-illustration-rtl@2x.png",
     platforms: ["linux", "win"]},
    {file: "chrome://browser/skin/customizableui/customize-illustration@2x.png",
     platforms: ["linux", "win"]},
    {file: "chrome://browser/skin/customizableui/info-icon-customizeTip@2x.png",
     platforms: ["linux", "win"]},
    {file: "chrome://browser/skin/customizableui/panelarrow-customizeTip@2x.png",
     platforms: ["linux", "win"]});
}

whitelist = new Set(whitelist.filter(item =>
  ("isFromDevTools" in item) == isDevtools &&
  (!item.skipNightly || !AppConstants.NIGHTLY_BUILD) &&
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

  // The following files are outside of the omni.ja file, so we only catch them
  // when testing on a non-packaged build.

  // toolkit/mozapps/extensions/nsBlocklistService.js
  "resource://app/blocklist.xml",

  // dom/media/gmp/GMPParent.cpp
  "resource://gre/gmp-clearkey/0.1/manifest.json",

  // Bug 1351669 - obsolete test file
  "resource://gre/res/test.properties",
]);
for (let entry of ignorableWhitelist) {
  whitelist.add(entry);
}

if (!isDevtools) {
  // services/sync/modules/service.js
  for (let module of ["addons.js", "bookmarks.js", "forms.js", "history.js",
                      "passwords.js", "prefs.js", "tabs.js",
                      "extension-storage.js"]) {
    whitelist.add("resource://services-sync/engines/" + module);
  }

  // intl/unicharutil/nsEntityConverter.h
  for (let name of ["html40Latin1", "html40Symbols", "html40Special", "mathml20"]) {
    whitelist.add("resource://gre/res/entityTables/" + name + ".properties");
  }
}

const gInterestingCategories = new Set([
  "agent-style-sheets", "addon-provider-module", "webextension-scripts",
  "webextension-schemas", "webextension-scripts-addon",
  "webextension-scripts-content", "webextension-scripts-devtools"
]);

var gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                 .getService(Ci.nsIChromeRegistry);
var gChromeMap = new Map();
var gOverrideMap = new Map();
var gReferencesFromCode = new Set();
var gComponentsSet = new Set();

var resHandler = Services.io.getProtocolHandler("resource")
                         .QueryInterface(Ci.nsIResProtocolHandler);
var gResourceMap = [];
function trackResourcePrefix(prefix) {
  let uri = Services.io.newURI("resource://" + prefix + "/");
  gResourceMap.unshift([prefix, resHandler.resolveURI(uri)]);
}
trackResourcePrefix("gre");
trackResourcePrefix("app");

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
      } else if (type == "resource") {
        trackResourcePrefix(argv[0]);
      } else if (type == "component") {
        gComponentsSet.add(argv[1]);
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
          gReferencesFromCode.add(convertToCodeURI(url));
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
          gReferencesFromCode.add(convertToCodeURI(url));
        } catch (e) {
          ok(false, "unexpected error while resolving this URI: " + url);
        }
      }
    }
  });
}

function parseCodeFile(fileUri) {
  return fetchFile(fileUri.spec).then(data => {
    let baseUri;
    for (let line of data.split("\n")) {
      let urls =
        line.match(/["'`]chrome:\/\/[a-zA-Z0-9 -]+\/(content|skin|locale)\/[^"'` ]*["'`]/g);
      if (!urls) {
        urls = line.match(/["']resource:\/\/[^"']+["']/g);
        if (urls && isDevtools &&
            /baseURI: "resource:\/\/devtools\//.test(line)) {
          baseUri = Services.io.newURI(urls[0].slice(1, -1));
          continue;
        }
      }
      if (!urls) {
        // If there's no absolute chrome URL, look for relative ones in
        // src and href attributes.
        let match = line.match("(?:src|href)=[\"']([^$&\"']+)");
        if (match && match[1]) {
          let url = Services.io.newURI(match[1], null, fileUri).spec;
          gReferencesFromCode.add(convertToCodeURI(url));
        }

        if (isDevtools) {
          let rules = [
            ["gcli", "resource://devtools/shared/gcli/source/lib/gcli"],
            ["devtools/client/locales", "chrome://devtools/locale"],
            ["devtools/shared/locales", "chrome://devtools-shared/locale"],
            ["devtools/shared/platform", "resource://devtools/shared/platform/chrome"],
            ["devtools", "resource://devtools"]
          ];

          match = line.match(/["']((?:devtools|gcli)\/[^\\#"']+)["']/);
          if (match && match[1]) {
            let path = match[1];
            for (let rule of rules) {
              if (path.startsWith(rule[0] + "/")) {
                path = path.replace(rule[0], rule[1]);
                if (!/\.(properties|js|jsm|json|css)$/.test(path))
                  path += ".js";
                gReferencesFromCode.add(path);
                break
              }
            }
          }

          match = line.match(/require\(['"](\.[^'"]+)['"]\)/);
          if (match && match[1]) {
            let url = match[1];
            url = Services.io.newURI(url, null, baseUri || fileUri).spec;
            url = convertToCodeURI(url);
            if (!/\.(properties|js|jsm|json|css)$/.test(url))
              url += ".js";
            if (url.startsWith("resource://")) {
              gReferencesFromCode.add(url);
            } else {
              // if we end up with a chrome:// url here, it's likely because
              // a baseURI to a resource:// path has been defined in another
              // .js file that is loaded in the same scope, we can't detect it.
            }
          }
        }
        continue;
      }

      for (let url of urls) {
        // Remove quotes.
        url = url.slice(1, -1);
        // Remove ? or \ trailing characters.
        if (url.endsWith("\\")) {
          url = url.slice(0, -1);
        }

        let pos = url.indexOf("?");
        if (pos != -1) {
          url = url.slice(0, pos);
        }

        // Make urls like chrome://browser/skin/ point to an actual file,
        // and remove the ref if any.
        url = Services.io.newURI(url).specIgnoringRef;

        if (isDevtools && line.includes("require(") &&
            !/\.(properties|js|jsm|json|css)$/.test(url))
          url += ".js";

        gReferencesFromCode.add(url);
      }
    }
  });
}

function convertToCodeURI(fileUri) {
  let baseUri = fileUri;
  let path = "";
  while (true) {
    let slashPos = baseUri.lastIndexOf("/", baseUri.length - 2);
    if (slashPos <= 0) {
      // File not accessible from chrome protocol, try resource://
      for (let res of gResourceMap) {
        if (fileUri.startsWith(res[1]))
          return fileUri.replace(res[1], "resource://" + res[0] + "/");
      }
      // Give up and return the original URL.
      return fileUri;
    }
    path = baseUri.slice(slashPos + 1) + path;
    baseUri = baseUri.slice(0, slashPos + 1);
    if (gChromeMap.has(baseUri))
      return gChromeMap.get(baseUri) + path;
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
    if (e.result != Components.results.NS_ERROR_FILE_NOT_FOUND &&
        e.result != Components.results.NS_ERROR_NOT_AVAILABLE) {
      todo(false, "Failed to check if " + aURI + "exists: " + e);
    }
  }
  return available > 0;
}

function findChromeUrlsFromArray(array, prefix) {
  // Find the first character of the prefix...
  for (let index = 0;
       (index = array.indexOf(prefix.charCodeAt(0), index)) != -1;
       ++index) {
    // Then ensure we actually have the whole prefix.
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
                       array.indexOf(")".charCodeAt(0), index),
                       array.indexOf("#".charCodeAt(0), index));
    let string = "";
    for ( ; index < end; ++index) {
      string += String.fromCharCode(array[index]);
    }

    // Only keep strings that look like real chrome or resource urls.
    if (/chrome:\/\/[a-zA-Z09 -]+\/(content|skin|locale)\//.test(string) ||
        /resource:\/\/gre.*\.[a-z]+/.test(string))
      gReferencesFromCode.add(string);
  }
}

add_task(async function checkAllTheFiles() {
  let libxulPath = OS.Constants.Path.libxul;
  if (AppConstants.platform != "macosx")
    libxulPath = OS.Constants.Path.libDir + "/" + libxulPath;
  let libxul = await OS.File.read(libxulPath);
  findChromeUrlsFromArray(libxul, "chrome://");
  findChromeUrlsFromArray(libxul, "resource://");
  // Handle NS_LITERAL_STRING.
  let uint16 = new Uint16Array(libxul.buffer);
  findChromeUrlsFromArray(uint16, "chrome://");
  findChromeUrlsFromArray(uint16, "resource://");

  const kCodeExtensions = [".xul", ".xml", ".xsl", ".js", ".jsm", ".html", ".xhtml"];

  let appDir = Services.dirsvc.get("GreD", Ci.nsIFile);
  // This asynchronously produces a list of URLs (sadly, mostly sync on our
  // test infrastructure because it runs against jarfiles there, and
  // our zipreader APIs are all sync)
  let uris = await generateURIsFromDirTree(appDir, [".css", ".manifest", ".json", ".jpg", ".png", ".gif", ".svg",  ".dtd", ".properties"].concat(kCodeExtensions));

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
  await Promise.all(manifestPromises);

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
  await Promise.all(allPromises);

  // Keep only chrome:// files, and filter out either the devtools paths or
  // the non-devtools paths:
  let devtoolsPrefixes = ["chrome://webide/",
                          "chrome://devtools",
                          "resource://devtools/",
                          "resource://app/modules/devtools",
                          "resource://gre/modules/devtools"];
  let chromeFiles = [];
  for (let uri of uris) {
    uri = convertToCodeURI(uri.spec);
    if ((uri.startsWith("chrome://") || uri.startsWith("resource://")) &&
        isDevtools == devtoolsPrefixes.some(prefix => uri.startsWith(prefix)))
      chromeFiles.push(uri);
  }

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

  let unreferencedFiles = chromeFiles.filter(f => {
    let rv = isUnreferenced(f);
    if (rv && f.startsWith("resource://app/"))
      rv = isUnreferenced(f.replace("resource://app/", "resource:///"));
    if (rv && /^resource:\/\/(?:app|gre)\/components\/[^/]+\.js$/.test(f))
      rv = !gComponentsSet.has(f.replace(/.*\//, ""));
    return rv;
  }).filter(notWhitelisted).sort();

  if (isDevtools) {
    // Bug 1351878 - handle devtools resource files
    unreferencedFiles = unreferencedFiles.filter(file => {
      if (file.startsWith("resource://")) {
        info("unreferenced devtools resource file: " + file);
        return false;
      }
      return true;
    });
  }

  unreferencedFiles = unreferencedFiles.filter(file => {
    // resource://app/features/ will only contain .xpi files when the test runs
    // on a packaged build, so the following two exceptions only matter when
    // running the test on a local non-packaged build.

    if (/resource:\/\/app\/features\/[^/]+\/bootstrap\.js/.test(file)) {
      info("not reporting feature boostrap file: " + file)
      return false;
    }
    // Bug 1351892 - can stop shipping these?
    if (/resource:\/\/app\/features\/[^/]+\/chrome\/skin\//.test(file)) {
      info("not reporting feature skin file that may be for another platform: " + file)
      return false;
    }
    return true;
  });

  is(unreferencedFiles.length, 0, "there should be no unreferenced files");
  for (let file of unreferencedFiles) {
    ok(false, "unreferenced file: " + file);
  }

  for (let file of whitelist) {
    if (ignorableWhitelist.has(file))
      info("ignored unused whitelist entry: " + file);
    else
      ok(false, "unused whitelist entry: " + file);
  }

  for (let file of gReferencesFromCode) {
    if (isDevtools != devtoolsPrefixes.some(prefix => file.startsWith(prefix)))
      continue;

    if ((file.startsWith("chrome://") || file.startsWith("resource://")) &&
        !chromeFileExists(file)) {
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
