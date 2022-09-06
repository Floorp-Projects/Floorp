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
  { sourceName: /codemirror\.css$/i, isFromDevTools: true },
  {
    sourceName: /devtools\/content\/debugger\/src\/components\/([A-z\/]+).css/i,
    isFromDevTools: true,
  },
  // Highlighter CSS uses a UA-only pseudo-class, see bug 985597.
  {
    sourceName: /highlighters\.css$/i,
    errorMessage: /Unknown pseudo-class.*moz-native-anonymous/i,
    isFromDevTools: true,
  },
  // UA-only media features.
  {
    sourceName: /\b(autocomplete-item)\.css$/,
    errorMessage: /Expected media feature name but found \u2018-moz.*/i,
    isFromDevTools: false,
    platforms: ["windows"],
  },
  {
    sourceName: /\b(contenteditable|EditorOverride|svg|forms|html|mathml|ua)\.css$/i,
    errorMessage: /Unknown pseudo-class.*-moz-/i,
    isFromDevTools: false,
  },
  {
    sourceName: /\b(minimal-xul|html|mathml|ua|forms|svg|manageDialog|autocomplete-item-shared|formautofill)\.css$/i,
    errorMessage: /Unknown property.*-moz-/i,
    isFromDevTools: false,
  },
  {
    sourceName: /(minimal-xul|xul)\.css$/i,
    errorMessage: /Unknown pseudo-class.*-moz-/i,
    isFromDevTools: false,
  },
  // Reserved to UA sheets unless layout.css.overflow-clip-box.enabled flipped to true.
  {
    sourceName: /(?:res|gre-resources)\/forms\.css$/i,
    errorMessage: /Unknown property.*overflow-clip-box/i,
    isFromDevTools: false,
  },
  // These variables are declared somewhere else, and error when we load the
  // files directly. They're all marked intermittent because their appearance
  // in the error console seems to not be consistent.
  {
    sourceName: /jsonview\/css\/general\.css$/i,
    intermittent: true,
    errorMessage: /Property contained reference to invalid variable.*color/i,
    isFromDevTools: true,
  },
  // PDF.js uses a property that is currently only supported in chrome.
  {
    sourceName: /web\/viewer\.css$/i,
    errorMessage: /Unknown property ‘text-size-adjust’\. {2}Declaration dropped\./i,
    isFromDevTools: false,
  },
  // PDF.js uses a property that is currently only supported in chrome.
  {
    sourceName: /web\/viewer\.css$/i,
    errorMessage: /Unknown property ‘forced-color-adjust’\. {2}Declaration dropped\./i,
    isFromDevTools: false,
  },
  {
    sourceName: /overlay\.css$/i,
    errorMessage: /Unknown pseudo-class.*moz-native-anonymous/i,
    isFromDevTools: false,
  },
];

if (!Services.prefs.getBoolPref("layout.css.color-mix.enabled")) {
  // Reserved to UA sheets unless layout.css.color-mix.enabled flipped to true.
  whitelist.push({
    sourceName: /\b(autocomplete-item)\.css$/,
    errorMessage: /Expected color but found \u2018color-mix\u2019./i,
    isFromDevTools: false,
    platforms: ["windows"],
  });
}

if (!Services.prefs.getBoolPref("layout.css.math-depth.enabled")) {
  // mathml.css UA sheet rule for math-depth.
  whitelist.push({
    sourceName: /\b(minimal-xul|mathml)\.css$/i,
    errorMessage: /Unknown property .*\bmath-depth\b/i,
    isFromDevTools: false,
  });
}

if (!Services.prefs.getBoolPref("layout.css.math-style.enabled")) {
  // mathml.css UA sheet rule for math-style.
  whitelist.push({
    sourceName: /(?:res|gre-resources)\/mathml\.css$/i,
    errorMessage: /Unknown property .*\bmath-style\b/i,
    isFromDevTools: false,
  });
}

if (!Services.prefs.getBoolPref("layout.css.scroll-anchoring.enabled")) {
  whitelist.push({
    sourceName: /webconsole\.css$/i,
    errorMessage: /Unknown property .*\boverflow-anchor\b/i,
    isFromDevTools: true,
  });
}

if (!Services.prefs.getBoolPref("layout.css.forced-colors.enabled")) {
  whitelist.push({
    sourceName: /pdf\.js\/web\/viewer\.css$/,
    errorMessage: /Expected media feature name but found ‘forced-colors’*/i,
    isFromDevTools: false,
  });
}

let propNameWhitelist = [
  // These custom properties are retrieved directly from CSSOM
  // in videocontrols.xml to get pre-defined style instead of computed
  // dimensions, which is why they are not referenced by CSS.
  { propName: "--clickToPlay-width", isFromDevTools: false },
  { propName: "--playButton-width", isFromDevTools: false },
  { propName: "--muteButton-width", isFromDevTools: false },
  { propName: "--castingButton-width", isFromDevTools: false },
  { propName: "--closedCaptionButton-width", isFromDevTools: false },
  { propName: "--fullscreenButton-width", isFromDevTools: false },
  { propName: "--durationSpan-width", isFromDevTools: false },
  { propName: "--durationSpan-width-long", isFromDevTools: false },
  { propName: "--positionDurationBox-width", isFromDevTools: false },
  { propName: "--positionDurationBox-width-long", isFromDevTools: false },

  // These variables are used in a shorthand, but the CSS parser deletes the values
  // when expanding the shorthands. See https://github.com/w3c/csswg-drafts/issues/2515
  { propName: "--bezier-diagonal-color", isFromDevTools: true },
  { propName: "--bezier-grid-color", isFromDevTools: true },
  { propName: "--page-border", isFromDevTools: false },

  // This variable is used from CSS embedded in JS in adjustableTitle.js
  { propName: "--icon-url", isFromDevTools: false },
];

// Add suffix to stylesheets' URI so that we always load them here and
// have them parsed. Add a random number so that even if we run this
// test multiple times, it would be unlikely to affect each other.
const kPathSuffix = "?always-parse-css-" + Math.random();

function dumpWhitelistItem(item) {
  return JSON.stringify(item, (key, value) => {
    return value instanceof RegExp ? value.toString() : value;
  });
}

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
    let catchAll = true;
    for (let prop of ["sourceName", "errorMessage"]) {
      if (whitelistItem.hasOwnProperty(prop)) {
        catchAll = false;
        if (!whitelistItem[prop].test(aErrorObject[prop] || "")) {
          matches = false;
          break;
        }
      }
    }
    if (catchAll) {
      ok(
        false,
        "A whitelist item is catching all errors. " +
          dumpWhitelistItem(whitelistItem)
      );
      continue;
    }
    if (matches) {
      whitelistItem.used = true;
      let { sourceName, errorMessage } = aErrorObject;
      info(
        `Ignored error "${errorMessage}" on ${sourceName} ` +
          "because of whitelist item " +
          dumpWhitelistItem(whitelistItem)
      );
      return true;
    }
  }
  return false;
}

var gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
  Ci.nsIChromeRegistry
);
var gChromeMap = new Map();

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
  let chromeFile = chromeUri + "gobbledygooknonexistentfile.reallynothere";
  let uri = Services.io.newURI(chromeFile);
  let fileUri = gChromeReg.convertChromeURL(uri);
  return fileUri.resolve(".");
}

function parseManifest(manifestUri) {
  return fetchFile(manifestUri.spec).then(data => {
    for (let line of data.split("\n")) {
      let [type, ...argv] = line.split(/\s+/);
      if (type == "content" || type == "skin") {
        let chromeUri = `chrome://${argv[0]}/${type}/`;
        gChromeMap.set(getBaseUriForChromeUri(chromeUri), chromeUri);
      } else if (type == "resource") {
        trackResourcePrefix(argv[0]);
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

function messageIsCSSError(msg) {
  // Only care about CSS errors generated by our iframe:
  if (
    msg instanceof Ci.nsIScriptError &&
    msg.category.includes("CSS") &&
    msg.sourceName.endsWith(kPathSuffix)
  ) {
    let sourceName = msg.sourceName.slice(0, -kPathSuffix.length);
    let msgInfo = { sourceName, errorMessage: msg.errorMessage };
    // Check if this error is whitelisted in whitelist
    if (!ignoredError(msgInfo)) {
      ok(false, `Got error message for ${sourceName}: ${msg.errorMessage}`);
      return true;
    }
  }
  return false;
}

let imageURIsToReferencesMap = new Map();
let customPropsToReferencesMap = new Map();

function neverMatches(mediaList) {
  const perPlatformMediaQueryMap = {
    macosx: ["(-moz-platform: macos)"],
    win: [
      "(-moz-platform: windows)",
      "(-moz-platform: windows-win7)",
      "(-moz-platform: windows-win8)",
      "(-moz-platform: windows-win10)",
    ],
    linux: ["(-moz-platform: linux)"],
    android: ["(-moz-platform: android)"],
  };
  for (let platform in perPlatformMediaQueryMap) {
    if (platform === AppConstants.platform) {
      continue;
    }
    if (perPlatformMediaQueryMap[platform].includes(mediaList.mediaText)) {
      // This query only matches on another platform that isn't ours.
      return true;
    }
  }
  return false;
}

function processCSSRules(container) {
  for (let rule of container.cssRules) {
    if (rule.media && neverMatches(rule.media)) {
      continue;
    }
    if (rule.styleSheet) {
      processCSSRules(rule.styleSheet); // @import
      continue;
    }
    if (rule.cssRules) {
      processCSSRules(rule); // @supports, @media, @layer (block), @keyframes
      continue;
    }
    if (!rule.style) {
      continue; // @layer (statement), @scroll-timeline, @font-feature-values, @counter-style
    }
    // Extract urls from the css text.
    // Note: CSSRule.style.cssText always has double quotes around URLs even
    //       when the original CSS file didn't.
    let cssText = rule.style.cssText;
    let urls = cssText.match(/url\("[^"]*"\)/g);
    // Extract props by searching all "--" preceded by "var(" or a non-word
    // character.
    let props = cssText.match(/(var\(|\W|^)(--[\w\-]+)/g);
    if (!urls && !props) {
      continue;
    }

    for (let url of urls || []) {
      // Remove the url(" prefix and the ") suffix.
      url = url.replace(/url\("(.*)"\)/, "$1");
      if (url.startsWith("data:")) {
        continue;
      }

      // Make the url absolute and remove the ref.
      let baseURI = Services.io.newURI(rule.parentStyleSheet.href);
      url = Services.io.newURI(url, null, baseURI).specIgnoringRef;

      // Store the image url along with the css file referencing it.
      let baseUrl = baseURI.spec.split("?always-parse-css")[0];
      if (!imageURIsToReferencesMap.has(url)) {
        imageURIsToReferencesMap.set(url, new Set([baseUrl]));
      } else {
        imageURIsToReferencesMap.get(url).add(baseUrl);
      }
    }

    for (let prop of props || []) {
      if (prop.startsWith("var(")) {
        prop = prop.substring(4);
        let prevValue = customPropsToReferencesMap.get(prop) || 0;
        customPropsToReferencesMap.set(prop, prevValue + 1);
      } else {
        // Remove the extra non-word character captured by the regular
        // expression if needed.
        if (prop[0] != "-") {
          prop = prop.substring(1);
        }
        if (!customPropsToReferencesMap.has(prop)) {
          customPropsToReferencesMap.set(prop, undefined);
        }
      }
    }
  }
}

function chromeFileExists(aURI) {
  let available = 0;
  try {
    let channel = NetUtil.newChannel({
      uri: aURI,
      loadUsingSystemPrincipal: true,
    });
    let stream = channel.open();
    let sstream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
      Ci.nsIScriptableInputStream
    );
    sstream.init(stream);
    available = sstream.available();
    sstream.close();
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
      dump("Checking " + aURI + ": " + e + "\n");
      Cu.reportError(e);
    }
  }
  return available > 0;
}

add_task(async function checkAllTheCSS() {
  // Since we later in this test use Services.console.getMessageArray(),
  // better to not have some messages from previous tests in the array.
  Services.console.reset();

  let appDir = Services.dirsvc.get("GreD", Ci.nsIFile);
  // This asynchronously produces a list of URLs (sadly, mostly sync on our
  // test infrastructure because it runs against jarfiles there, and
  // our zipreader APIs are all sync)
  let uris = await generateURIsFromDirTree(appDir, [".css", ".manifest"]);

  // Create a clean iframe to load all the files into. This needs to live at a
  // chrome URI so that it's allowed to load and parse any styles.
  let testFile = getRootDirectory(gTestPath) + "dummy_page.html";
  let { HiddenFrame } = ChromeUtils.import(
    "resource://gre/modules/HiddenFrame.jsm"
  );
  let hiddenFrame = new HiddenFrame();
  let win = await hiddenFrame.get();
  let iframe = win.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "html:iframe"
  );
  win.document.documentElement.appendChild(iframe);
  let iframeLoaded = BrowserTestUtils.waitForEvent(iframe, "load", true);
  iframe.contentWindow.location = testFile;
  await iframeLoaded;
  let doc = iframe.contentWindow.document;
  iframe.contentWindow.docShell.cssErrorReportingEnabled = true;

  // Parse and remove all manifests from the list.
  // NOTE that this must be done before filtering out devtools paths
  // so that all chrome paths can be recorded.
  let manifestURIs = [];
  uris = uris.filter(uri => {
    if (uri.pathQueryRef.endsWith(".manifest")) {
      manifestURIs.push(uri);
      return false;
    }
    return true;
  });
  // Wait for all manifest to be parsed
  await throttledMapPromises(manifestURIs, parseManifest);

  // filter out either the devtools paths or the non-devtools paths:
  let isDevtools = SimpleTest.harnessParameters.subsuite == "devtools";
  let devtoolsPathBits = ["devtools"];
  uris = uris.filter(
    uri => isDevtools == devtoolsPathBits.some(path => uri.spec.includes(path))
  );

  let loadCSS = chromeUri =>
    new Promise(resolve => {
      let linkEl, onLoad, onError;
      onLoad = e => {
        processCSSRules(linkEl.sheet);
        resolve();
        linkEl.removeEventListener("load", onLoad);
        linkEl.removeEventListener("error", onError);
      };
      onError = e => {
        ok(
          false,
          "Loading " + linkEl.getAttribute("href") + " threw an error!"
        );
        resolve();
        linkEl.removeEventListener("load", onLoad);
        linkEl.removeEventListener("error", onError);
      };
      linkEl = doc.createElement("link");
      linkEl.setAttribute("rel", "stylesheet");
      linkEl.setAttribute("type", "text/css");
      linkEl.addEventListener("load", onLoad);
      linkEl.addEventListener("error", onError);
      linkEl.setAttribute("href", chromeUri + kPathSuffix);
      doc.head.appendChild(linkEl);
    });

  // We build a list of promises that get resolved when their respective
  // files have loaded and produced no errors.
  const kInContentCommonCSS = "chrome://global/skin/in-content/common.css";
  let allPromises = uris
    .map(uri => convertToCodeURI(uri.spec))
    .filter(uri => uri !== kInContentCommonCSS);

  // Make sure chrome://global/skin/in-content/common.css is loaded before other
  // stylesheets in order to guarantee the --in-content variables can be
  // correctly referenced.
  if (allPromises.length !== uris.length) {
    await loadCSS(kInContentCommonCSS);
  }

  // Wait for all the files to have actually loaded:
  await throttledMapPromises(allPromises, loadCSS);

  // Check if all the files referenced from CSS actually exist.
  // Files in browser/ should never be referenced outside browser/.
  for (let [image, references] of imageURIsToReferencesMap) {
    if (!chromeFileExists(image)) {
      for (let ref of references) {
        ok(false, "missing " + image + " referenced from " + ref);
      }
    }

    let imageHost = image.split("/")[2];
    if (imageHost == "browser") {
      for (let ref of references) {
        let refHost = ref.split("/")[2];
        if (!["activity-stream", "browser"].includes(refHost)) {
          ok(
            false,
            "browser file " + image + " referenced outside browser in " + ref
          );
        }
      }
    }
  }

  // Check if all the properties that are defined are referenced.
  for (let [prop, refCount] of customPropsToReferencesMap) {
    if (!refCount) {
      let ignored = false;
      for (let item of propNameWhitelist) {
        if (item.propName == prop && isDevtools == item.isFromDevTools) {
          item.used = true;
          if (
            !item.platforms ||
            item.platforms.includes(AppConstants.platform)
          ) {
            ignored = true;
          }
          break;
        }
      }
      if (!ignored) {
        ok(false, "custom property `" + prop + "` is not referenced");
      }
    }
  }

  let messages = Services.console.getMessageArray();
  // Count errors (the test output will list actual issues for us, as well
  // as the ok(false) in messageIsCSSError.
  let errors = messages.filter(messageIsCSSError);
  is(
    errors.length,
    0,
    "All the styles (" + allPromises.length + ") loaded without errors."
  );

  // Confirm that all whitelist rules have been used.
  function checkWhitelist(list) {
    for (let item of list) {
      if (
        !item.used &&
        isDevtools == item.isFromDevTools &&
        (!item.platforms || item.platforms.includes(AppConstants.platform)) &&
        !item.intermittent
      ) {
        ok(false, "Unused whitelist item: " + dumpWhitelistItem(item));
      }
    }
  }
  checkWhitelist(whitelist);
  checkWhitelist(propNameWhitelist);

  // Clean up to avoid leaks:
  doc.head.innerHTML = "";
  doc = null;
  iframe.remove();
  iframe = null;
  win = null;
  hiddenFrame.destroy();
  hiddenFrame = null;
  imageURIsToReferencesMap = null;
  customPropsToReferencesMap = null;
});
