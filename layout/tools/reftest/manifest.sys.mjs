/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { globals } from "resource://reftest/globals.sys.mjs";

const {
  NS_GFXINFO_CONTRACTID,

  TYPE_REFTEST_EQUAL,
  TYPE_REFTEST_NOTEQUAL,
  TYPE_LOAD,
  TYPE_SCRIPT,
  TYPE_PRINT,

  EXPECTED_PASS,
  EXPECTED_FAIL,
  EXPECTED_RANDOM,
  EXPECTED_FUZZY,

  PREF_BOOLEAN,
  PREF_STRING,
  PREF_INTEGER,

  FOCUS_FILTER_NEEDS_FOCUS_TESTS,
  FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS,

  g,
} = globals;

import { NetUtil } from "resource://gre/modules/NetUtil.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX =
  "@mozilla.org/network/protocol;1?name=";

const RE_PROTOCOL = /^\w+:/;
const RE_PREF_ITEM = /^(|test-|ref-)pref\((.+?),(.*)\)$/;

export function ReadTopManifest(aFileURL, aFilter, aManifestID) {
  var url = g.ioService.newURI(aFileURL);
  if (!url) {
    throw new Error("Expected a file or http URL for the manifest.");
  }

  g.manifestsLoaded = {};
  ReadManifest(url, aFilter, aManifestID);
}

// Note: If you materially change the reftest manifest parsing,
// please keep the parser in layout/tools/reftest/__init__.py in sync.
// eslint-disable-next-line complexity
function ReadManifest(aURL, aFilter, aManifestID) {
  // Ensure each manifest is only read once. This assumes that manifests that
  // are included with filters will be read via their include before they are
  // read directly in the case of a duplicate
  if (g.manifestsLoaded.hasOwnProperty(aURL.spec)) {
    if (g.manifestsLoaded[aURL.spec] === null) {
      return;
    }
    aFilter = [aFilter[0], aFilter[1], true];
  }
  g.manifestsLoaded[aURL.spec] = aFilter[1];

  var listURL = aURL;
  var channel = NetUtil.newChannel({
    uri: aURL,
    loadUsingSystemPrincipal: true,
  });
  try {
    var inputStream = channel.open();
  } catch (e) {
    g.logger.error("failed to open manifest at : " + aURL.spec);
    throw new Error(e);
  }
  if (channel instanceof Ci.nsIHttpChannel && channel.responseStatus != 200) {
    g.logger.error("HTTP ERROR : " + channel.responseStatus);
  }
  var streamBuf = getStreamContent(inputStream);
  inputStream.close();
  var lines = streamBuf.split(/\n|\r|\r\n/);

  // The sandbox for fails-if(), etc., condition evaluation. This is not
  // always required and so is created on demand.
  var sandbox;
  function GetOrCreateSandbox() {
    if (!sandbox) {
      sandbox = BuildConditionSandbox(aURL);
    }
    return sandbox;
  }

  var lineNo = 0;
  var urlprefix = "";
  var defaults = [];
  var defaultTestPrefSettings = [],
    defaultRefPrefSettings = [];
  if (g.compareRetainedDisplayLists) {
    AddRetainedDisplayListTestPrefs(
      GetOrCreateSandbox(),
      defaultTestPrefSettings,
      defaultRefPrefSettings
    );
  }
  for (var str of lines) {
    ++lineNo;
    if (str.charAt(0) == "#") {
      continue;
    } // entire line was a comment
    var i = str.search(/\s+#/);
    if (i >= 0) {
      str = str.substring(0, i);
    }
    // strip leading and trailing whitespace
    str = str.replace(/^\s*/, "").replace(/\s*$/, "");
    if (!str || str == "") {
      continue;
    }
    var items = str.split(/\s+/); // split on whitespace

    if (items[0] == "url-prefix") {
      if (items.length != 2) {
        throw new Error(
          "url-prefix requires one url in manifest file " +
            aURL.spec +
            " line " +
            lineNo
        );
      }
      urlprefix = items[1];
      continue;
    }

    if (items[0] == "defaults") {
      items.shift();
      defaults = items;
      continue;
    }

    var expected_status = EXPECTED_PASS;
    var allow_silent_fail = false;
    var minAsserts = 0;
    var maxAsserts = 0;
    var needs_focus = false;
    var slow = false;
    var skip = false;
    var testPrefSettings = defaultTestPrefSettings.concat();
    var refPrefSettings = defaultRefPrefSettings.concat();
    var fuzzy_delta = { min: 0, max: 2 };
    var fuzzy_pixels = { min: 0, max: 1 };
    var chaosMode = false;
    var wrCapture = { test: false, ref: false };
    var nonSkipUsed = false;
    var noAutoFuzz = false;

    var origLength = items.length;
    items = defaults.concat(items);
    var modifiers = [...items];
    while (
      items[0].match(
        /^(fails|needs-focus|random|skip|asserts|slow|require-or|silentfail|pref|test-pref|ref-pref|fuzzy|chaos-mode|wr-capture|wr-capture-ref|noautofuzz)/
      )
    ) {
      var item = items.shift();
      var stat;
      var cond;
      var m = item.match(/^(fails|random|skip|silentfail)-if(\(.*\))$/);
      if (m) {
        stat = m[1];
        // Note: m[2] contains the parentheses, and we want them.
        cond = Cu.evalInSandbox(m[2], GetOrCreateSandbox());
      } else if (item.match(/^(fails|random|skip)$/)) {
        stat = item;
        cond = true;
      } else if (item == "needs-focus") {
        needs_focus = true;
        cond = false;
      } else if ((m = item.match(/^asserts\((\d+)(-\d+)?\)$/))) {
        cond = false;
        minAsserts = Number(m[1]);
        maxAsserts = m[2] == undefined ? minAsserts : Number(m[2].substring(1));
      } else if ((m = item.match(/^asserts-if\((.*?),(\d+)(-\d+)?\)$/))) {
        cond = false;
        if (Cu.evalInSandbox("(" + m[1] + ")", GetOrCreateSandbox())) {
          minAsserts = Number(m[2]);
          maxAsserts =
            m[3] == undefined ? minAsserts : Number(m[3].substring(1));
        }
      } else if (item == "slow") {
        cond = false;
        slow = true;
      } else if ((m = item.match(/^require-or\((.*?)\)$/))) {
        var args = m[1].split(/,/);
        if (args.length != 2) {
          throw new Error(
            "Error in manifest file " +
              aURL.spec +
              " line " +
              lineNo +
              ": wrong number of args to require-or"
          );
        }
        var [precondition_str, fallback_action] = args;
        var preconditions = precondition_str.split(/&&/);
        cond = false;
        for (var precondition of preconditions) {
          if (precondition === "debugMode") {
            // Currently unimplemented. Requires asynchronous
            // JSD call + getting an event while no JS is running
            stat = fallback_action;
            cond = true;
            break;
          } else if (precondition === "true") {
            // For testing
          } else {
            // Unknown precondition. Assume it is unimplemented.
            stat = fallback_action;
            cond = true;
            break;
          }
        }
      } else if ((m = item.match(/^slow-if\((.*?)\)$/))) {
        cond = false;
        if (Cu.evalInSandbox("(" + m[1] + ")", GetOrCreateSandbox())) {
          slow = true;
        }
      } else if (item == "silentfail") {
        cond = false;
        allow_silent_fail = true;
      } else if ((m = item.match(RE_PREF_ITEM))) {
        cond = false;
        if (
          !AddPrefSettings(
            m[1],
            m[2],
            m[3],
            GetOrCreateSandbox(),
            testPrefSettings,
            refPrefSettings
          )
        ) {
          throw new Error(
            "Error in pref value in manifest file " +
              aURL.spec +
              " line " +
              lineNo
          );
        }
      } else if ((m = item.match(/^fuzzy\((\d+)-(\d+),(\d+)-(\d+)\)$/))) {
        cond = false;
        expected_status = EXPECTED_FUZZY;
        fuzzy_delta = ExtractRange(m, 1);
        fuzzy_pixels = ExtractRange(m, 3);
      } else if (
        (m = item.match(/^fuzzy-if\((.*?),(\d+)-(\d+),(\d+)-(\d+)\)$/))
      ) {
        cond = false;
        if (Cu.evalInSandbox("(" + m[1] + ")", GetOrCreateSandbox())) {
          expected_status = EXPECTED_FUZZY;
          fuzzy_delta = ExtractRange(m, 2);
          fuzzy_pixels = ExtractRange(m, 4);
        }
      } else if (item == "chaos-mode") {
        cond = false;
        chaosMode = true;
      } else if (item == "wr-capture") {
        cond = false;
        wrCapture.test = true;
      } else if (item == "wr-capture-ref") {
        cond = false;
        wrCapture.ref = true;
      } else if (item == "noautofuzz") {
        cond = false;
        noAutoFuzz = true;
      } else {
        throw new Error(
          "Error in manifest file " +
            aURL.spec +
            " line " +
            lineNo +
            ": unexpected item " +
            item
        );
      }

      if (stat != "skip") {
        nonSkipUsed = true;
      }

      if (cond) {
        if (stat == "fails") {
          expected_status = EXPECTED_FAIL;
        } else if (stat == "random") {
          expected_status = EXPECTED_RANDOM;
        } else if (stat == "skip") {
          skip = true;
        } else if (stat == "silentfail") {
          allow_silent_fail = true;
        }
      }
    }

    if (items.length > origLength) {
      // Implies we broke out of the loop before we finished processing
      // defaults. This means defaults contained an invalid token.
      throw new Error(
        "Error in manifest file " +
          aURL.spec +
          " line " +
          lineNo +
          ": invalid defaults token '" +
          items[0] +
          "'"
      );
    }

    if (minAsserts > maxAsserts) {
      throw new Error(
        "Bad range in manifest file " + aURL.spec + " line " + lineNo
      );
    }

    var runHttp = false;
    var httpDepth;
    if (items[0] == "HTTP") {
      runHttp = aURL.scheme == "file"; // We can't yet run the local HTTP server
      // for non-local reftests.
      httpDepth = 0;
      items.shift();
    } else if (items[0].match(/HTTP\(\.\.(\/\.\.)*\)/)) {
      // Accept HTTP(..), HTTP(../..), HTTP(../../..), etc.
      runHttp = aURL.scheme == "file"; // We can't yet run the local HTTP server
      // for non-local reftests.
      httpDepth = (items[0].length - 5) / 3;
      items.shift();
    }

    // do not prefix the url for include commands or urls specifying
    // a protocol
    if (urlprefix && items[0] != "include") {
      if (items.length > 1 && !items[1].match(RE_PROTOCOL)) {
        items[1] = urlprefix + items[1];
      }
      if (items.length > 2 && !items[2].match(RE_PROTOCOL)) {
        items[2] = urlprefix + items[2];
      }
    }

    var principal = Services.scriptSecurityManager.createContentPrincipal(
      aURL,
      {}
    );

    if (items[0] == "include") {
      if (items.length != 2) {
        throw new Error(
          "Error in manifest file " +
            aURL.spec +
            " line " +
            lineNo +
            ": incorrect number of arguments to include"
        );
      }
      if (runHttp) {
        throw new Error(
          "Error in manifest file " +
            aURL.spec +
            " line " +
            lineNo +
            ": use of include with http"
        );
      }

      // If the expected_status is EXPECTED_PASS (the default) then allow
      // the include. If 'skip' is true, that means there was a skip
      // or skip-if annotation (with a true condition) on this include
      // statement, so we should skip the include. Any other expected_status
      // is disallowed since it's nonintuitive as to what the intended
      // effect is.
      if (nonSkipUsed) {
        throw new Error(
          "Error in manifest file " +
            aURL.spec +
            " line " +
            lineNo +
            ": include statement with annotation other than 'skip' or 'skip-if'"
        );
      } else if (skip) {
        g.logger.info(
          "Skipping included manifest at " +
            aURL.spec +
            " line " +
            lineNo +
            " due to matching skip condition"
        );
      } else {
        // poor man's assertion
        if (expected_status != EXPECTED_PASS) {
          throw new Error(
            "Error in manifest file parsing code: we should never get expected_status=" +
              expected_status +
              " when nonSkipUsed=false (from " +
              aURL.spec +
              " line " +
              lineNo +
              ")"
          );
        }

        var incURI = g.ioService.newURI(items[1], null, listURL);
        Services.scriptSecurityManager.checkLoadURIWithPrincipal(
          principal,
          incURI,
          Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
        );

        // Cannot use nsIFile or similar to manipulate the manifest ID; although it appears
        // path-like, it does not refer to an actual path in the filesystem.
        var newManifestID = aManifestID;
        var included = items[1];
        // Remove included manifest file name.
        // eg. dir1/dir2/reftest.list -> dir1/dir2
        var pos = included.lastIndexOf("/");
        if (pos <= 0) {
          included = "";
        } else {
          included = included.substring(0, pos);
        }
        // Simplify references to parent directories.
        // eg. dir1/dir2/../dir3 -> dir1/dir3
        while (included.startsWith("../")) {
          pos = newManifestID.lastIndexOf("/");
          if (pos < 0) {
            pos = 0;
          }
          newManifestID = newManifestID.substring(0, pos);
          included = included.substring(3);
        }
        // Use a new manifest ID if the included manifest is in a different directory.
        if (included.length) {
          if (newManifestID.length) {
            newManifestID = newManifestID + "/" + included;
          } else {
            // parent directory includes may refer to the topsrcdir
            newManifestID = included;
          }
        }
        ReadManifest(incURI, aFilter, newManifestID);
      }
    } else if (items[0] == TYPE_LOAD || items[0] == TYPE_SCRIPT) {
      let type = items[0];
      if (items.length != 2) {
        throw new Error(
          "Error in manifest file " +
            aURL.spec +
            " line " +
            lineNo +
            ": incorrect number of arguments to " +
            type
        );
      }
      if (type == TYPE_LOAD && expected_status != EXPECTED_PASS) {
        throw new Error(
          "Error in manifest file " +
            aURL.spec +
            " line " +
            lineNo +
            ": incorrect known failure type for load test"
        );
      }
      AddTestItem(
        {
          type,
          expected: expected_status,
          manifest: aURL.spec,
          manifestID: TestIdentifier(aURL.spec, aManifestID),
          allowSilentFail: allow_silent_fail,
          minAsserts,
          maxAsserts,
          needsFocus: needs_focus,
          slow,
          skip,
          prefSettings1: testPrefSettings,
          prefSettings2: refPrefSettings,
          fuzzyMinDelta: fuzzy_delta.min,
          fuzzyMaxDelta: fuzzy_delta.max,
          fuzzyMinPixels: fuzzy_pixels.min,
          fuzzyMaxPixels: fuzzy_pixels.max,
          runHttp,
          httpDepth,
          url1: items[1],
          url2: null,
          chaosMode,
          wrCapture,
          noAutoFuzz,
          modifiers,
        },
        aFilter,
        aManifestID
      );
    } else if (
      items[0] == TYPE_REFTEST_EQUAL ||
      items[0] == TYPE_REFTEST_NOTEQUAL ||
      items[0] == TYPE_PRINT
    ) {
      if (items.length != 3) {
        throw new Error(
          "Error in manifest file " +
            aURL.spec +
            " line " +
            lineNo +
            ": incorrect number of arguments to " +
            items[0]
        );
      }

      if (
        items[0] == TYPE_REFTEST_NOTEQUAL &&
        expected_status == EXPECTED_FUZZY &&
        (fuzzy_delta.min > 0 || fuzzy_pixels.min > 0)
      ) {
        throw new Error(
          "Error in manifest file " +
            aURL.spec +
            " line " +
            lineNo +
            ": minimum fuzz must be zero for tests of type " +
            items[0]
        );
      }

      let type = items[0];
      if (g.compareRetainedDisplayLists) {
        type = TYPE_REFTEST_EQUAL;

        // We expect twice as many assertion failures when comparing
        // tests because we run each test twice.
        minAsserts *= 2;
        maxAsserts *= 2;

        // Skip the test if it is expected to fail in both modes.
        // It would unexpectedly "pass" in comparison mode mode when
        // comparing the two failures, which is not a useful result.
        if (
          expected_status === EXPECTED_FAIL ||
          expected_status === EXPECTED_RANDOM
        ) {
          skip = true;
        }
      }

      AddTestItem(
        {
          type,
          expected: expected_status,
          manifest: aURL.spec,
          manifestID: TestIdentifier(aURL.spec, aManifestID),
          allowSilentFail: allow_silent_fail,
          minAsserts,
          maxAsserts,
          needsFocus: needs_focus,
          slow,
          skip,
          prefSettings1: testPrefSettings,
          prefSettings2: refPrefSettings,
          fuzzyMinDelta: fuzzy_delta.min,
          fuzzyMaxDelta: fuzzy_delta.max,
          fuzzyMinPixels: fuzzy_pixels.min,
          fuzzyMaxPixels: fuzzy_pixels.max,
          runHttp,
          httpDepth,
          url1: items[1],
          url2: items[2],
          chaosMode,
          wrCapture,
          noAutoFuzz,
          modifiers,
        },
        aFilter,
        aManifestID
      );
    } else {
      throw new Error(
        "Error in manifest file " +
          aURL.spec +
          " line " +
          lineNo +
          ": unknown test type " +
          items[0]
      );
    }
  }
}

// Read all available data from an input stream and return it
// as a string.
function getStreamContent(inputStream) {
  var streamBuf = "";
  var sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  sis.init(inputStream);

  var available;
  while ((available = sis.available()) != 0) {
    streamBuf += sis.read(available);
  }

  return streamBuf;
}

// Build the sandbox for fails-if(), etc., condition evaluation.
function BuildConditionSandbox(aURL) {
  var sandbox = new Cu.Sandbox(aURL.spec);
  sandbox.isDebugBuild = g.debug.isDebugBuild;
  sandbox.isCoverageBuild = g.isCoverageBuild;

  sandbox.xulRuntime = {};

  var gfxInfo =
    NS_GFXINFO_CONTRACTID in Cc &&
    Cc[NS_GFXINFO_CONTRACTID].getService(Ci.nsIGfxInfo);
  let readGfxInfo = function (obj, key) {
    if (g.contentGfxInfo && key in g.contentGfxInfo) {
      return g.contentGfxInfo[key];
    }
    return obj[key];
  };

  try {
    var windowProtocol = readGfxInfo(gfxInfo, "windowProtocol");
    sandbox.wayland = windowProtocol == "wayland";
  } catch (e) {
    sandbox.wayland = false;
  }

  sandbox.mozinfo = Services.prefs.getStringPref("sandbox.mozinfo", null);
  sandbox.os_version = sandbox.mozinfo.os_version;

  sandbox.d3d11 = g.windowUtils.layerManagerType == "Direct3D 11";
  sandbox.swgl = g.windowUtils.layerManagerType.startsWith(
    "WebRender (Software"
  );

  // Shortcuts for widget toolkits.
  sandbox.Android = Services.appinfo.OS == "Android";
  sandbox.cocoaWidget = Services.appinfo.widgetToolkit == "cocoa";
  sandbox.gtkWidget = Services.appinfo.widgetToolkit == "gtk";
  sandbox.winWidget = Services.appinfo.widgetToolkit == "windows";

  sandbox.is64Bit = Services.appinfo.is64Bit;

  // Use this to annotate reftests that fail in drawSnapshot, but
  // the reason hasn't been investigated (or fixed) yet.
  sandbox.useDrawSnapshot = g.useDrawSnapshot;

  // GeckoView is currently uniquely identified by "android + e10s" but
  // we might want to make this condition more precise in the future.
  sandbox.geckoview = sandbox.Android && g.browserIsRemote;

  if (sandbox.Android) {
    sandbox.AndroidVersion = Services.sysinfo.getPropertyAsInt32("version");

    sandbox.emulator = readGfxInfo(gfxInfo, "adapterDeviceID").includes(
      "Android Emulator"
    );
    sandbox.device = !sandbox.emulator;
  }

  // Some reftests need extra fuzz on the Android 13 Pixel 5 devices.
  sandbox.Android13 = sandbox.AndroidVersion == "33";

  sandbox.AddressSanitizer = AppConstants.ASAN;
  sandbox.ThreadSanitizer = AppConstants.TSAN;
  sandbox.webrtc = AppConstants.MOZ_WEBRTC;

  sandbox.release_or_beta = AppConstants.RELEASE_OR_BETA;

  var hh = Cc[NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX + "http"].getService(
    Ci.nsIHttpProtocolHandler
  );

  // Set OSX to be the Mac OS X version, as an integer, or undefined
  // for other platforms.  The integer is formed by 100 times the
  // major version plus the minor version, so 1006 for 10.6, 1010 for
  // 10.10, etc.
  var osxmatch = /Mac OS X (\d+).(\d+)$/.exec(hh.oscpu);
  sandbox.OSX = osxmatch
    ? parseInt(osxmatch[1]) * 100 + parseInt(osxmatch[2])
    : undefined;

  // config specific prefs
  sandbox.appleSilicon = Services.prefs.getBoolPref(
    "sandbox.apple_silicon",
    false
  );

  sandbox.prefs = Cu.cloneInto(
    {
      getBoolPref(p) {
        return Services.prefs.getBoolPref(p);
      },
      getIntPref(p) {
        return Services.prefs.getIntPref(p);
      },
    },
    sandbox,
    { cloneFunctions: true }
  );

  // Running in a test-verify session?
  sandbox.verify = Services.prefs.getBoolPref("reftest.verify", false);

  // Running with a variant enabled?
  sandbox.fission = Services.appinfo.fissionAutostart;

  if (!g.dumpedConditionSandbox) {
    g.logger.info(
      "Dumping representation of sandbox which can be used for expectation annotations"
    );
    for (let entry of Object.entries(Cu.waiveXrays(sandbox)).sort((a, b) =>
      a[0].localeCompare(b[0])
    )) {
      let value =
        typeof entry[1] === "object" ? JSON.stringify(entry[1]) : entry[1];
      g.logger.info(`    ${entry[0]}: ${value}`);
    }
    g.dumpedConditionSandbox = true;
  }

  return sandbox;
}

function AddRetainedDisplayListTestPrefs(
  aSandbox,
  aTestPrefSettings,
  aRefPrefSettings
) {
  AddPrefSettings(
    "test-",
    "layout.display-list.retain",
    "true",
    aSandbox,
    aTestPrefSettings,
    aRefPrefSettings
  );
  AddPrefSettings(
    "ref-",
    "layout.display-list.retain",
    "false",
    aSandbox,
    aTestPrefSettings,
    aRefPrefSettings
  );
}

function AddPrefSettings(
  aWhere,
  aPrefName,
  aPrefValExpression,
  aSandbox,
  aTestPrefSettings,
  aRefPrefSettings
) {
  var prefVal = Cu.evalInSandbox("(" + aPrefValExpression + ")", aSandbox);
  var prefType;
  var valType = typeof prefVal;
  if (valType == "boolean") {
    prefType = PREF_BOOLEAN;
  } else if (valType == "string") {
    prefType = PREF_STRING;
  } else if (valType == "number" && parseInt(prefVal) == prefVal) {
    prefType = PREF_INTEGER;
  } else {
    return false;
  }
  var setting = { name: aPrefName, type: prefType, value: prefVal };

  if (
    g.compareRetainedDisplayLists &&
    aPrefName != "layout.display-list.retain"
  ) {
    // ref-pref() is ignored, test-pref() and pref() are added to both
    if (aWhere != "ref-") {
      aTestPrefSettings.push(setting);
      aRefPrefSettings.push(setting);
    }
  } else {
    if (aWhere != "ref-") {
      aTestPrefSettings.push(setting);
    }
    if (aWhere != "test-") {
      aRefPrefSettings.push(setting);
    }
  }
  return true;
}

function ExtractRange(matches, startIndex) {
  return {
    min: Number(matches[startIndex]),
    max: Number(matches[startIndex + 1]),
  };
}

function ServeTestBase(aURL, depth) {
  var listURL = aURL.QueryInterface(Ci.nsIFileURL);
  var directory = listURL.file.parent;

  // Allow serving a tree that's an ancestor of the directory containing
  // the files so that they can use resources in ../ (etc.).
  var dirPath = "/";
  while (depth > 0) {
    dirPath = "/" + directory.leafName + dirPath;
    directory = directory.parent;
    --depth;
  }

  g.count++;
  var path = "/" + Date.now() + "/" + g.count;
  g.server.registerDirectory(path + "/", directory);
  // this one is needed so tests can use example.org urls for cross origin testing
  g.server.registerDirectory("/", directory);

  return g.ioService.newURI(
    "http://localhost:" + g.httpServerPort + path + dirPath
  );
}

export function CreateUrls(test) {
  let manifestURL = g.ioService.newURI(test.manifest);

  let testbase = manifestURL;
  if (test.runHttp) {
    testbase = ServeTestBase(manifestURL, test.httpDepth);
  }

  let testbasePrincipal = Services.scriptSecurityManager.createContentPrincipal(
    testbase,
    {}
  );
  Services.perms.addFromPrincipal(
    testbasePrincipal,
    "allowXULXBL",
    Services.perms.ALLOW_ACTION
  );

  function FileToURI(file) {
    if (file === null) {
      return file;
    }

    var testURI = g.ioService.newURI(file, null, testbase);
    let isChromeOrViewSource =
      testURI.scheme == "chrome" || testURI.scheme == "view-source";
    let principal = isChromeOrViewSource
      ? Services.scriptSecurityManager.getSystemPrincipal()
      : Services.scriptSecurityManager.createContentPrincipal(manifestURL, {});
    Services.scriptSecurityManager.checkLoadURIWithPrincipal(
      principal,
      testURI,
      Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT
    );
    return testURI;
  }

  let files = [test.url1, test.url2];
  [test.url1, test.url2] = files.map(FileToURI);

  return test;
}

function TestIdentifier(aUrl, aManifestID) {
  // Construct a platform-independent and location-independent test identifier for
  // a url; normally the identifier looks like a posix-compliant relative file
  // path.
  // Test urls may be simple file names, chrome: urls with full paths, about:blank, etc.
  if (aUrl.startsWith("about:") || aUrl.startsWith("data:")) {
    return aUrl;
  }
  var pos = aUrl.lastIndexOf("/");
  var url = pos < 0 ? aUrl : aUrl.substring(pos + 1);
  return aManifestID + "/" + url;
}

function AddTestItem(aTest, aFilter, aManifestID) {
  if (!aFilter) {
    aFilter = [null, [], false];
  }

  var identifier = TestIdentifier(aTest.url1, aManifestID);
  if (aTest.url2 !== null) {
    identifier = [
      identifier,
      aTest.type,
      TestIdentifier(aTest.url2, aManifestID),
    ];
  }

  var { url1, url2 } = CreateUrls(Object.assign({}, aTest));

  var globalFilter = aFilter[0];
  var manifestFilter = aFilter[1];
  var invertManifest = aFilter[2];
  if (globalFilter && !globalFilter.test(url1.spec)) {
    if (url2 === null) {
      return;
    }
    if (globalFilter && !globalFilter.test(url2.spec)) {
      return;
    }
  }
  if (manifestFilter && !(invertManifest ^ manifestFilter.test(url1.spec))) {
    if (url2 === null) {
      return;
    }
    if (manifestFilter && !(invertManifest ^ manifestFilter.test(url2.spec))) {
      return;
    }
  }
  if (
    g.focusFilterMode == FOCUS_FILTER_NEEDS_FOCUS_TESTS &&
    !aTest.needsFocus
  ) {
    return;
  }
  if (
    g.focusFilterMode == FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS &&
    aTest.needsFocus
  ) {
    return;
  }

  aTest.identifier = identifier;
  g.urls.push(aTest);
  // Periodically log progress to avoid no-output timeout on slow platforms.
  // No-output timeouts during manifest parsing have been a problem for
  // jsreftests on Android/debug. Any logging resets the no-output timer,
  // even debug logging which is normally not displayed.
  if (g.urls.length % 5000 == 0) {
    g.logger.debug(g.urls.length + " tests found...");
  }
}
