/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Note to run this test similar to try server, you need to run:
// ./mach package
// ./mach mochitest --appname dist <path to test>

// Slow on asan builds.
requestLongerTimeout(5);

var isDevtools = SimpleTest.harnessParameters.subsuite == "devtools";

// This list should contain only path prefixes. It is meant to stop the test
// from reporting things that *are* referenced, but for which the test can't
// find any reference because the URIs are constructed programatically.
// If you need to allowlist specific files, please use the 'allowlist' object.
var gExceptionPaths = [
  "resource://app/defaults/settings/blocklists/",
  "resource://app/defaults/settings/security-state/",
  "resource://app/defaults/settings/main/",
  "resource://app/defaults/preferences/",
  "resource://gre/modules/commonjs/",
  "resource://gre/defaults/pref/",

  // These chrome resources are referenced using relative paths from JS files.
  "chrome://global/content/certviewer/components/",

  // https://github.com/mozilla/activity-stream/issues/3053
  "chrome://activity-stream/content/data/content/tippytop/images/",
  "chrome://activity-stream/content/data/content/tippytop/favicons/",
  // These resources are referenced by messages delivered through Remote Settings
  "chrome://activity-stream/content/data/content/assets/remote/",
  "chrome://activity-stream/content/data/content/assets/mobile-download-qr-new-user-cn.svg",
  "chrome://activity-stream/content/data/content/assets/mobile-download-qr-existing-user-cn.svg",
  "chrome://activity-stream/content/data/content/assets/person-typing.svg",
  "chrome://browser/content/assets/moz-vpn.svg",
  "chrome://browser/content/assets/vpn-logo.svg",
  "chrome://browser/content/assets/focus-promo.png",
  "chrome://browser/content/assets/klar-qr-code.svg",

  // toolkit/components/pdfjs/content/build/pdf.js
  "resource://pdf.js/web/images/",

  // Exclude the form autofill path that has been moved out of the extensions to
  // toolkit, see bug 1691821.
  "resource://gre-resources/autofill/",

  // Exclude all search-extensions because they aren't referenced by filename
  "resource://search-extensions/",

  // Exclude all services-automation because they are used through webdriver
  "resource://gre/modules/services-automation/",

  // Paths from this folder are constructed in NetErrorParent.sys.mjs based on
  // the type of cert or net error the user is encountering.
  "chrome://global/content/neterror/supportpages/",

  // Points to theme preview images, which are defined in browser/ but only used
  // in toolkit/mozapps/extensions/content/aboutaddons.js.
  "resource://usercontext-content/builtin-themes/",

  // Page data schemas are referenced programmatically.
  "chrome://browser/content/pagedata/schemas/",

  // Nimbus schemas are referenced programmatically.
  "resource://nimbus/schemas/",

  // Activity stream schemas are referenced programmatically.
  "resource://activity-stream/schemas",

  // Localization file added programatically in FeatureCallout.sys.mjs
  "resource://app/localization/en-US/browser/featureCallout.ftl",

  // CSS files are referenced inside JS in an html template
  "chrome://browser/content/aboutlogins/components/",
];

// These are not part of the omni.ja file, so we find them only when running
// the test on a non-packaged build.
if (AppConstants.platform == "macosx") {
  gExceptionPaths.push("resource://gre/res/cursors/");
  gExceptionPaths.push("resource://gre/res/touchbar/");
}

if (AppConstants.MOZ_BACKGROUNDTASKS) {
  // These preferences are active only when we're in background task mode.
  gExceptionPaths.push("resource://gre/defaults/backgroundtasks/");
  gExceptionPaths.push("resource://app/defaults/backgroundtasks/");
  // `BackgroundTask_*.sys.mjs` are loaded at runtime by `app --backgroundtask id ...`.
  gExceptionPaths.push("resource://gre/modules/backgroundtasks/");
  gExceptionPaths.push("resource://app/modules/backgroundtasks/");
}

if (AppConstants.NIGHTLY_BUILD) {
  // This is nightly-only debug tool.
  gExceptionPaths.push(
    "chrome://browser/content/places/interactionsViewer.html"
  );
}

// Each allowlist entry should have a comment indicating which file is
// referencing the listed file in a way that the test can't detect, or a
// bug number to remove or use the file if it is indeed currently unreferenced.
var allowlist = [
  // security/manager/pki/resources/content/device_manager.js
  { file: "chrome://pippki/content/load_device.xhtml" },

  // The l10n build system can't package string files only for some platforms.
  // See bug 1339424 for why this is hard to fix.
  {
    file: "chrome://global/locale/fallbackMenubar.properties",
    platforms: ["linux", "win"],
  },
  {
    file: "resource://gre/localization/en-US/toolkit/printing/printDialogs.ftl",
    platforms: ["linux", "macosx"],
  },

  // This file is referenced by the build system to generate the
  // Firefox .desktop entry. See bug 1824327 (and perhaps bug 1526672)
  {
    file: "resource://app/localization/en-US/browser/linuxDesktopEntry.ftl",
  },

  // toolkit/content/aboutRights-unbranded.xhtml doesn't use aboutRights.css
  { file: "chrome://global/skin/aboutRights.css", skipUnofficial: true },

  // devtools/client/inspector/bin/dev-server.js
  {
    file: "chrome://devtools/content/inspector/markup/markup.xhtml",
    isFromDevTools: true,
  },

  // used by devtools/client/memory/index.xhtml
  { file: "chrome://global/content/third_party/d3/d3.js" },

  // SpiderMonkey parser API, currently unused in browser/ and toolkit/
  { file: "resource://gre/modules/reflect.sys.mjs" },

  // extensions/pref/autoconfig/src/nsReadConfig.cpp
  { file: "resource://gre/defaults/autoconfig/prefcalls.js" },

  // browser/components/preferences/moreFromMozilla.js
  // These files URLs are constructed programatically at run time.
  {
    file: "chrome://browser/content/preferences/more-from-mozilla-qr-code-simple.svg",
  },
  {
    file: "chrome://browser/content/preferences/more-from-mozilla-qr-code-simple-cn.svg",
  },

  { file: "resource://gre/greprefs.js" },

  // layout/mathml/nsMathMLChar.cpp
  { file: "resource://gre/res/fonts/mathfontSTIXGeneral.properties" },
  { file: "resource://gre/res/fonts/mathfontUnicode.properties" },

  // toolkit/mozapps/extensions/AddonContentPolicy.cpp
  { file: "resource://gre/localization/en-US/toolkit/global/cspErrors.ftl" },

  // The l10n build system can't package string files only for some platforms.
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/mac/accessible.properties",
    platforms: ["linux", "win"],
  },
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/mac/intl.properties",
    platforms: ["linux", "win"],
  },
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/mac/platformKeys.properties",
    platforms: ["linux", "win"],
  },
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/unix/accessible.properties",
    platforms: ["macosx", "win"],
  },
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/unix/intl.properties",
    platforms: ["macosx", "win"],
  },
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/unix/platformKeys.properties",
    platforms: ["macosx", "win"],
  },
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/win/accessible.properties",
    platforms: ["linux", "macosx"],
  },
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/win/intl.properties",
    platforms: ["linux", "macosx"],
  },
  {
    file: "resource://gre/chrome/en-US/locale/en-US/global-platform/win/platformKeys.properties",
    platforms: ["linux", "macosx"],
  },

  // Files from upstream library
  { file: "resource://pdf.js/web/debugger.mjs" },
  { file: "resource://pdf.js/web/debugger.css" },

  // Starting from here, files in the allowlist are bugs that need fixing.
  // Bug 1339424 (wontfix?)
  {
    file: "chrome://browser/locale/taskbar.properties",
    platforms: ["linux", "macosx"],
  },
  // Bug 1344267
  { file: "chrome://remote/content/marionette/test_dialog.properties" },
  { file: "chrome://remote/content/marionette/test_dialog.xhtml" },
  { file: "chrome://remote/content/marionette/test_menupopup.xhtml" },
  { file: "chrome://remote/content/marionette/test_no_xul.xhtml" },
  { file: "chrome://remote/content/marionette/test.xhtml" },
  // Bug 1348559
  { file: "chrome://pippki/content/resetpassword.xhtml" },
  // Bug 1337345
  { file: "resource://gre/modules/Manifest.sys.mjs" },
  // Bug 1494170
  // (The references to these files are dynamically generated, so the test can't
  // find the references)
  {
    file: "chrome://devtools/skin/images/aboutdebugging-firefox-aurora.svg",
    isFromDevTools: true,
  },
  {
    file: "chrome://devtools/skin/images/aboutdebugging-firefox-beta.svg",
    isFromDevTools: true,
  },
  {
    file: "chrome://devtools/skin/images/aboutdebugging-firefox-release.svg",
    isFromDevTools: true,
  },
  { file: "chrome://devtools/skin/images/next.svg", isFromDevTools: true },

  // Bug 1526672
  {
    file: "resource://app/localization/en-US/browser/touchbar/touchbar.ftl",
    platforms: ["linux", "win"],
  },
  // Referenced by the webcompat system addon for localization
  { file: "resource://gre/localization/en-US/toolkit/about/aboutCompat.ftl" },

  // dom/media/mediacontrol/MediaControlService.cpp
  { file: "resource://gre/localization/en-US/dom/media.ftl" },

  // dom/xml/nsXMLPrettyPrinter.cpp
  { file: "resource://gre/localization/en-US/dom/XMLPrettyPrint.ftl" },

  // tookit/mozapps/update/BackgroundUpdate.sys.mjs
  {
    file: "resource://gre/localization/en-US/toolkit/updates/backgroundupdate.ftl",
  },
  // toolkit/mozapps/defaultagent/Notification.cpp
  // toolkit/mozapps/defaultagent/ScheduledTask.cpp
  // Bug 1854425 - referenced by default browser agent which is not detected
  {
    file: "resource://app/localization/en-US/browser/backgroundtasks/defaultagent.ftl",
  },

  // Bug 1713242 - referenced by aboutThirdParty.html which is only for Windows
  {
    file: "resource://gre/localization/en-US/toolkit/about/aboutThirdParty.ftl",
    platforms: ["linux", "macosx"],
  },
  // Bug 1854618 - referenced by aboutWebauthn.html which is only for Linux and Mac
  {
    file: "resource://gre/localization/en-US/toolkit/about/aboutWebauthn.ftl",
    platforms: ["win", "android"],
  },
  // Bug 1973834 - referenced by aboutWindowsMessages.html which is only for Windows
  {
    file: "resource://gre/localization/en-US/toolkit/about/aboutWindowsMessages.ftl",
    platforms: ["linux", "macosx"],
  },
  // Bug 1721741:
  // (The references to these files are dynamically generated, so the test can't
  // find the references)
  { file: "chrome://browser/content/screenshots/copied-notification.svg" },

  // toolkit/xre/MacRunFromDmgUtils.mm
  { file: "resource://gre/localization/en-US/toolkit/global/run-from-dmg.ftl" },

  // Referenced by screenshots extension
  { file: "chrome://browser/content/screenshots/cancel.svg" },
  { file: "chrome://browser/content/screenshots/copy.svg" },
  { file: "chrome://browser/content/screenshots/download.svg" },
  { file: "chrome://browser/content/screenshots/download-white.svg" },
];

if (AppConstants.NIGHTLY_BUILD && AppConstants.platform != "win") {
  // This path is refereneced in nsFxrCommandLineHandler.cpp, which is only
  // compiled in Windows. This path is allowed so that non-Windows builds
  // can access the FxR UI via --chrome rather than --fxr (which includes VR-
  // specific functionality)
  allowlist.push({ file: "chrome://fxr/content/fxrui.html" });
}

if (AppConstants.platform == "android") {
  // The l10n build system can't package string files only for some platforms.
  // Referenced by aboutGlean.html
  allowlist.push({
    file: "resource://gre/localization/en-US/toolkit/about/aboutGlean.ftl",
  });
}

if (AppConstants.MOZ_UPDATE_AGENT && !AppConstants.MOZ_BACKGROUNDTASKS) {
  // Task scheduling is only used for background updates right now.
  allowlist.push({
    file: "resource://gre/modules/TaskScheduler.sys.mjs",
  });
}

allowlist = new Set(
  allowlist
    .filter(
      item =>
        "isFromDevTools" in item == isDevtools &&
        (!item.skipUnofficial || !AppConstants.MOZILLA_OFFICIAL) &&
        (!item.platforms || item.platforms.includes(AppConstants.platform))
    )
    .map(item => item.file)
);

const ignorableAllowlist = new Set([
  // The following files are outside of the omni.ja file, so we only catch them
  // when testing on a non-packaged build.

  // toolkit/mozapps/extensions/nsBlocklistService.js
  "resource://app/blocklist.xml",

  // dom/media/gmp/GMPParent.cpp
  "resource://gre/gmp-clearkey/0.1/manifest.json",

  // Bug 1351669 - obsolete test file
  "resource://gre/res/test.properties",
]);
for (let entry of ignorableAllowlist) {
  allowlist.add(entry);
}

if (!isDevtools) {
  // services/sync/modules/service.sys.mjs
  for (let module of [
    "addons.sys.mjs",
    "bookmarks.sys.mjs",
    "forms.sys.mjs",
    "history.sys.mjs",
    "passwords.sys.mjs",
    "prefs.sys.mjs",
    "tabs.sys.mjs",
    "extension-storage.sys.mjs",
  ]) {
    allowlist.add("resource://services-sync/engines/" + module);
  }
  // resource://devtools/shared/worker/loader.js,
  // resource://devtools/shared/loader/builtin-modules.js
  if (!AppConstants.ENABLE_WEBDRIVER) {
    allowlist.add("resource://gre/modules/jsdebugger.sys.mjs");
  }
}

if (AppConstants.MOZ_CODE_COVERAGE) {
  allowlist.add(
    "chrome://remote/content/marionette/PerTestCoverageUtils.sys.mjs"
  );
}

const gInterestingCategories = new Set([
  "addon-provider-module",
  "webextension-modules",
  "webextension-scripts",
  "webextension-schemas",
  "webextension-scripts-addon",
  "webextension-scripts-content",
  "webextension-scripts-devtools",
]);

var gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
  Ci.nsIChromeRegistry
);
var gChromeMap = new Map();
var gOverrideMap = new Map();
var gComponentsSet = new Set();

// In this map when the value is a Set of URLs, the file is referenced if any
// of the files in the Set is referenced.
// When the value is null, the file is referenced unconditionally.
// When the value is a string, "allowlist-direct" means that we have not found
// any reference in the code, but have a matching allowlist entry for this file.
// "allowlist" means that the file is indirectly allowlisted, ie. a allowlisted
// file causes this file to be referenced.
var gReferencesFromCode = new Map();

var resHandler = Services.io
  .getProtocolHandler("resource")
  .QueryInterface(Ci.nsIResProtocolHandler);
var gResourceMap = [];
function trackResourcePrefix(prefix) {
  let uri = Services.io.newURI("resource://" + prefix + "/");
  gResourceMap.unshift([prefix, resHandler.resolveURI(uri)]);
}
trackResourcePrefix("gre");
trackResourcePrefix("app");

function getBaseUriForChromeUri(chromeUri) {
  let chromeFile = chromeUri + "nonexistentfile.reallynothere";
  let uri = Services.io.newURI(chromeFile);
  let fileUri = gChromeReg.convertChromeURL(uri);
  return fileUri.resolve(".");
}

function trackChromeUri(uri) {
  gChromeMap.set(getBaseUriForChromeUri(uri), uri);
}

// formautofill registers resource://formautofill/ and
// chrome://formautofill/content/ dynamically at runtime.
// Bug 1480276 is about addressing this without this hard-coding.
trackResourcePrefix("autofill");
trackChromeUri("chrome://formautofill/content/");

function parseManifest(manifestUri) {
  return fetchFile(manifestUri.spec).then(data => {
    for (let line of data.split("\n")) {
      let [type, ...argv] = line.split(/\s+/);
      if (type == "content" || type == "skin" || type == "locale") {
        let chromeUri = `chrome://${argv[0]}/${type}/`;
        // The webcompat reporter's locale directory may not exist if
        // the addon is preffed-off, and since it's a hack until we
        // get bz1425104 landed, we'll just skip it for now.
        if (chromeUri === "chrome://report-site-issue/locale/") {
          gChromeMap.set("chrome://report-site-issue/locale/", true);
        } else {
          trackChromeUri(chromeUri);
        }
      } else if (type == "override" || type == "overlay") {
        // Overlays aren't really overrides, but behave the same in
        // that the overlay is only referenced if the original xul
        // file is referenced somewhere.
        let os = "os=" + Services.appinfo.OS;
        if (!argv.some(s => s.startsWith("os=") && s != os)) {
          gOverrideMap.set(
            Services.io.newURI(argv[1]).specIgnoringRef,
            Services.io.newURI(argv[0]).specIgnoringRef
          );
        }
      } else if (type == "category" && gInterestingCategories.has(argv[0])) {
        gReferencesFromCode.set(argv[2], null);
      } else if (type == "resource") {
        trackResourcePrefix(argv[0]);
      } else if (type == "component") {
        gComponentsSet.add(argv[1]);
      }
    }
  });
}

// If the given URI is a webextension manifest, extract files used by
// any of its APIs (scripts, icons, style sheets, theme images).
// Returns the passed in URI if the manifest is not a webextension
// manifest, null otherwise.
async function parseJsonManifest(uri) {
  uri = Services.io.newURI(convertToCodeURI(uri.spec));

  let raw = await fetchFile(uri.spec);
  let data;
  try {
    data = JSON.parse(raw);
  } catch (ex) {
    return uri;
  }

  // Simplistic test for whether this is a webextension manifest:
  if (data.manifest_version !== 2) {
    return uri;
  }

  if (data.background?.scripts) {
    for (let bgscript of data.background.scripts) {
      gReferencesFromCode.set(uri.resolve(bgscript), null);
    }
  }

  if (data.icons) {
    for (let icon of Object.values(data.icons)) {
      gReferencesFromCode.set(uri.resolve(icon), null);
    }
  }

  if (data.experiment_apis) {
    for (let api of Object.values(data.experiment_apis)) {
      if (api.parent && api.parent.script) {
        let script = uri.resolve(api.parent.script);
        gReferencesFromCode.set(script, null);
      }

      if (api.schema) {
        gReferencesFromCode.set(uri.resolve(api.schema), null);
      }
    }
  }

  if (data.theme_experiment && data.theme_experiment.stylesheet) {
    let stylesheet = uri.resolve(data.theme_experiment.stylesheet);
    gReferencesFromCode.set(stylesheet, null);
  }

  for (let themeKey of ["theme", "dark_theme"]) {
    if (data?.[themeKey]?.images?.additional_backgrounds) {
      for (let background of data[themeKey].images.additional_backgrounds) {
        gReferencesFromCode.set(uri.resolve(background), null);
      }
    }
  }

  return null;
}

function addCodeReference(url, fromURI) {
  let from = convertToCodeURI(fromURI.spec);

  // Ignore self references.
  if (url == from) {
    return;
  }

  let ref;
  if (gReferencesFromCode.has(url)) {
    ref = gReferencesFromCode.get(url);
    if (ref === null) {
      return;
    }
  } else {
    ref = new Set();
    gReferencesFromCode.set(url, ref);
  }
  ref.add(from);
}

function listCodeReferences(refs) {
  let refList = [];
  if (refs) {
    for (let ref of refs) {
      refList.push(ref);
    }
  }
  return refList.join(",");
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
          addCodeReference(convertToCodeURI(url), fileUri);
        }
        continue;
      }

      for (let url of urls) {
        // Remove the url(" prefix and the ") suffix.
        url = url
          .replace(/url\(([^)]*)\)/, "$1")
          .replace(/^"(.*)"$/, "$1")
          .replace(/^'(.*)'$/, "$1");
        if (url.startsWith("data:")) {
          continue;
        }

        try {
          url = Services.io.newURI(url, null, fileUri).specIgnoringRef;
          addCodeReference(convertToCodeURI(url), fileUri);
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
      let urls = line.match(
        /["'`]chrome:\/\/[a-zA-Z0-9-]+\/(content|skin|locale)\/[^"'` ]*["'`]/g
      );

      if (!urls) {
        urls = line.match(/["']resource:\/\/[^"']+["']/g);
        if (
          urls &&
          isDevtools &&
          /baseURI: "resource:\/\/devtools\//.test(line)
        ) {
          baseUri = Services.io.newURI(urls[0].slice(1, -1));
          continue;
        }
      }

      if (!urls) {
        urls = line.match(/[a-z0-9_\/-]+\.ftl/i);
        if (urls) {
          urls = urls[0];
          let grePrefix = Services.io.newURI(
            "resource://gre/localization/en-US/"
          );
          let appPrefix = Services.io.newURI(
            "resource://app/localization/en-US/"
          );

          let grePrefixUrl = Services.io.newURI(urls, null, grePrefix).spec;
          let appPrefixUrl = Services.io.newURI(urls, null, appPrefix).spec;

          addCodeReference(grePrefixUrl, fileUri);
          addCodeReference(appPrefixUrl, fileUri);
          continue;
        }
      }

      if (!urls) {
        // If there's no absolute chrome URL, look for relative ones in
        // src and href attributes.
        let match = line.match("(?:src|href)=[\"']([^$&\"']+)");
        if (match && match[1]) {
          let url = Services.io.newURI(match[1], null, fileUri).spec;
          addCodeReference(convertToCodeURI(url), fileUri);
        }

        // This handles `import` lines which may be multi-line.
        // We have an ESLint rule, `import/no-unassigned-import` which prevents
        // using bare `import "foo.js"`, so we don't need to handle that case
        // here.
        match = line.match(/from\W*['"](.*?)['"]/);
        if (match?.[1]) {
          let url = match[1];
          url = Services.io.newURI(url, null, baseUri || fileUri).spec;
          url = convertToCodeURI(url);
          addCodeReference(url, fileUri);
        }

        if (isDevtools) {
          let rules = [
            ["devtools/client/locales", "chrome://devtools/locale"],
            ["devtools/shared/locales", "chrome://devtools-shared/locale"],
            [
              "devtools/shared/platform",
              "resource://devtools/shared/platform/chrome",
            ],
            ["devtools", "resource://devtools"],
          ];

          match = line.match(/["']((?:devtools)\/[^\\#"']+)["']/);
          if (match && match[1]) {
            let path = match[1];
            for (let rule of rules) {
              if (path.startsWith(rule[0] + "/")) {
                path = path.replace(rule[0], rule[1]);
                if (!/\.(properties|js|jsm|mjs|json|css)$/.test(path)) {
                  path += ".js";
                }
                addCodeReference(path, fileUri);
                break;
              }
            }
          }

          match = line.match(/require\(['"](\.[^'"]+)['"]\)/);
          if (match && match[1]) {
            let url = match[1];
            url = Services.io.newURI(url, null, baseUri || fileUri).spec;
            url = convertToCodeURI(url);
            if (!/\.(properties|js|jsm|mjs|json|css)$/.test(url)) {
              url += ".js";
            }
            if (url.startsWith("resource://")) {
              addCodeReference(url, fileUri);
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
        try {
          url = Services.io.newURI(url).specIgnoringRef;
        } catch (e) {
          continue;
        }

        if (
          isDevtools &&
          line.includes("require(") &&
          !/\.(properties|js|jsm|mjs|json|css)$/.test(url)
        ) {
          url += ".js";
        }

        addCodeReference(url, fileUri);
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
        if (fileUri.startsWith(res[1])) {
          return fileUri.replace(res[1], "resource://" + res[0] + "/");
        }
      }
      // Give up and return the original URL.
      return fileUri;
    }
    path = baseUri.slice(slashPos + 1) + path;
    baseUri = baseUri.slice(0, slashPos + 1);
    if (gChromeMap.has(baseUri)) {
      return gChromeMap.get(baseUri) + path;
    }
  }
}

async function chromeFileExists(aURI) {
  try {
    return await PerfTestHelpers.checkURIExists(aURI);
  } catch (e) {
    todo(false, `Failed to check if ${aURI} exists: ${e}`);
    return false;
  }
}

function findChromeUrlsFromArray(array, prefix) {
  // Find the first character of the prefix...
  for (
    let index = 0;
    (index = array.indexOf(prefix.charCodeAt(0), index)) != -1;
    ++index
  ) {
    // Then ensure we actually have the whole prefix.
    let found = true;
    for (let i = 1; i < prefix.length; ++i) {
      if (array[index + i] != prefix.charCodeAt(i)) {
        found = false;
        break;
      }
    }
    if (!found) {
      continue;
    }

    // C strings are null terminated, but " also terminates urls
    // (nsIndexedToHTML.cpp contains an HTML fragment with several chrome urls)
    // Let's also terminate the string on the # character to skip references.
    let end = Math.min(
      array.indexOf(0, index),
      array.indexOf('"'.charCodeAt(0), index),
      array.indexOf(")".charCodeAt(0), index),
      array.indexOf("#".charCodeAt(0), index)
    );
    let string = "";
    for (; index < end; ++index) {
      string += String.fromCharCode(array[index]);
    }

    // Only keep strings that look like real chrome or resource urls.
    if (
      /chrome:\/\/[a-zA-Z09-]+\/(content|skin|locale)\//.test(string) ||
      /resource:\/\/[a-zA-Z09-]*\/.*\.[a-z]+/.test(string)
    ) {
      gReferencesFromCode.set(string, null);
    }
  }
}

add_task(async function checkAllTheFiles() {
  TestUtils.assertPackagedBuild();

  const libxul = await IOUtils.read(PathUtils.xulLibraryPath);
  findChromeUrlsFromArray(libxul, "chrome://");
  findChromeUrlsFromArray(libxul, "resource://");
  // Handle NS_LITERAL_STRING.
  let uint16 = new Uint16Array(libxul.buffer);
  findChromeUrlsFromArray(uint16, "chrome://");
  findChromeUrlsFromArray(uint16, "resource://");

  const kCodeExtensions = [
    ".xml",
    ".xsl",
    ".mjs",
    ".js",
    ".jsm",
    ".json",
    ".html",
    ".xhtml",
  ];

  let appDir = Services.dirsvc.get("GreD", Ci.nsIFile);
  // This asynchronously produces a list of URLs (sadly, mostly sync on our
  // test infrastructure because it runs against jarfiles there, and
  // our zipreader APIs are all sync)
  let uris = await generateURIsFromDirTree(
    appDir,
    [
      ".css",
      ".manifest",
      ".jpg",
      ".png",
      ".gif",
      ".svg",
      ".ftl",
      ".dtd",
      ".properties",
    ].concat(kCodeExtensions)
  );

  // Parse and remove all manifests from the list.
  // NOTE that this must be done before filtering out devtools paths
  // so that all chrome paths can be recorded.
  let manifestURIs = [];
  let jsonManifests = [];
  uris = uris.filter(uri => {
    let path = uri.pathQueryRef;
    if (path.endsWith(".manifest")) {
      manifestURIs.push(uri);
      return false;
    } else if (path.endsWith("/manifest.json")) {
      jsonManifests.push(uri);
      return false;
    }

    return true;
  });

  // Wait for all manifest to be parsed
  await PerfTestHelpers.throttledMapPromises(manifestURIs, parseManifest);

  for (let jsm of Components.manager.getComponentJSMs()) {
    gReferencesFromCode.set(jsm, null);
  }
  for (let esModule of Components.manager.getComponentESModules()) {
    gReferencesFromCode.set(esModule, null);
  }

  // manifest.json is a common name, it is used for WebExtension manifests
  // but also for other things.  To tell them apart, we have to actually
  // read the contents.  This will populate gExtensionRoots with all
  // embedded extension APIs, and return any manifest.json files that aren't
  // webextensions.
  let nonWebextManifests = (
    await Promise.all(jsonManifests.map(parseJsonManifest))
  ).filter(uri => !!uri);
  uris.push(...nonWebextManifests);

  // We build a list of promises that get resolved when their respective
  // files have loaded and produced no errors.
  let allPromises = [];

  for (let uri of uris) {
    let path = uri.pathQueryRef;
    if (path.endsWith(".css")) {
      allPromises.push([parseCSSFile, uri]);
    } else if (kCodeExtensions.some(ext => path.endsWith(ext))) {
      allPromises.push([parseCodeFile, uri]);
    }
  }

  // Wait for all the files to have actually loaded:
  await PerfTestHelpers.throttledMapPromises(allPromises, ([task, uri]) =>
    task(uri)
  );

  // Keep only chrome:// files, and filter out either the devtools paths or
  // the non-devtools paths:
  let devtoolsPrefixes = [
    "chrome://devtools",
    "resource://devtools/",
    "resource://devtools-shared-images/",
    "resource://devtools-highlighter-styles/",
    "resource://app/modules/devtools",
    "resource://gre/modules/devtools",
    "resource://app/localization/en-US/startup/aboutDevTools.ftl",
    "resource://app/localization/en-US/devtools/",
  ];
  let hasDevtoolsPrefix = uri =>
    devtoolsPrefixes.some(prefix => uri.startsWith(prefix));
  let chromeFiles = [];
  for (let uri of uris) {
    uri = convertToCodeURI(uri.spec);
    if (
      (uri.startsWith("chrome://") || uri.startsWith("resource://")) &&
      isDevtools == hasDevtoolsPrefix(uri)
    ) {
      chromeFiles.push(uri);
    }
  }

  if (isDevtools) {
    // chrome://devtools/skin/devtools-browser.css is included from browser.xhtml
    gReferencesFromCode.set(AppConstants.BROWSER_CHROME_URL, null);
    // devtools' css is currently included from browser.css, see bug 1204810.
    gReferencesFromCode.set("chrome://browser/skin/browser.css", null);
  }

  let isUnreferenced = file => {
    if (gExceptionPaths.some(e => file.startsWith(e))) {
      return false;
    }
    if (gReferencesFromCode.has(file)) {
      let refs = gReferencesFromCode.get(file);
      if (refs === null) {
        return false;
      }
      for (let ref of refs) {
        if (isDevtools) {
          if (
            ref.startsWith("resource://app/components/") ||
            (file.startsWith("chrome://") && ref.startsWith("resource://"))
          ) {
            return false;
          }
        }

        if (gReferencesFromCode.has(ref)) {
          let refType = gReferencesFromCode.get(ref);
          if (
            refType === null || // unconditionally referenced
            refType == "allowlist" ||
            refType == "allowlist-direct"
          ) {
            return false;
          }
        }
      }
    }
    return !gOverrideMap.has(file) || isUnreferenced(gOverrideMap.get(file));
  };

  let unreferencedFiles = chromeFiles;

  let removeReferenced = useAllowlist => {
    let foundReference = false;
    unreferencedFiles = unreferencedFiles.filter(f => {
      let rv = isUnreferenced(f);
      if (rv && f.startsWith("resource://app/")) {
        rv = isUnreferenced(f.replace("resource://app/", "resource:///"));
      }
      if (rv && /^resource:\/\/(?:app|gre)\/components\/[^/]+\.js$/.test(f)) {
        rv = !gComponentsSet.has(f.replace(/.*\//, ""));
      }
      if (!rv) {
        foundReference = true;
        if (useAllowlist) {
          info(
            "indirectly allowlisted file: " +
              f +
              " used from " +
              listCodeReferences(gReferencesFromCode.get(f))
          );
        }
        gReferencesFromCode.set(f, useAllowlist ? "allowlist" : null);
      }
      return rv;
    });
    return foundReference;
  };
  // First filter out the files that are referenced.
  while (removeReferenced(false)) {
    // As long as removeReferenced returns true, some files have been marked
    // as referenced, so we need to run it again.
  }
  // Marked as referenced the files that have been explicitly allowed.
  unreferencedFiles = unreferencedFiles.filter(file => {
    if (allowlist.has(file)) {
      allowlist.delete(file);
      gReferencesFromCode.set(file, "allowlist-direct");
      return false;
    }
    return true;
  });
  // Run the process again, this time when more files are marked as referenced,
  // it's a consequence of the allowlist.
  while (removeReferenced(true)) {
    // As long as removeReferenced returns true, we need to run it again.
  }

  unreferencedFiles.sort();

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

  is(unreferencedFiles.length, 0, "there should be no unreferenced files");
  for (let file of unreferencedFiles) {
    let refs = gReferencesFromCode.get(file);
    if (refs === undefined) {
      ok(false, "unreferenced file: " + file);
    } else {
      let refList = listCodeReferences(refs);
      let msg = "file only referenced from unreferenced files: " + file;
      if (refList) {
        msg += " referenced from " + refList;
      }
      ok(false, msg);
    }
  }

  for (let file of allowlist) {
    if (ignorableAllowlist.has(file)) {
      info("ignored unused allowlist entry: " + file);
    } else {
      ok(false, "unused allowlist entry: " + file);
    }
  }

  for (let [file, refs] of gReferencesFromCode) {
    if (
      isDevtools != devtoolsPrefixes.some(prefix => file.startsWith(prefix))
    ) {
      continue;
    }

    if (
      (file.startsWith("chrome://") || file.startsWith("resource://")) &&
      !(await chromeFileExists(file))
    ) {
      // Ignore chrome prefixes that have been automatically expanded.
      let pathParts =
        file.match("chrome://([^/]+)/content/([^/.]+).xul") ||
        file.match("chrome://([^/]+)/skin/([^/.]+).css");
      if (pathParts && pathParts[1] == pathParts[2]) {
        continue;
      }

      // TODO: bug 1349010 - add a allowlist and make this reliable enough
      // that we could make the test fail when this catches something new.
      let refList = listCodeReferences(refs);
      let msg = "missing file: " + file;
      if (refList) {
        msg += " referenced from " + refList;
      }
      info(msg);
    }
  }
});
