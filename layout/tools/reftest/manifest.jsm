/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["ReadTopManifest"];

var CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("chrome://reftest/content/globals.jsm", this);
CU.import("chrome://reftest/content/reftest.jsm", this);
CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://gre/modules/NetUtil.jsm");

const NS_SCRIPTSECURITYMANAGER_CONTRACTID = "@mozilla.org/scriptsecuritymanager;1";
const NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX = "@mozilla.org/network/protocol;1?name=";
const NS_XREAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";

const RE_PROTOCOL = /^\w+:/;
const RE_PREF_ITEM = /^(|test-|ref-)pref\((.+?),(.*)\)$/;


function ReadTopManifest(aFileURL, aFilter)
{
    var url = g.ioService.newURI(aFileURL);
    if (!url)
        throw "Expected a file or http URL for the manifest.";

    g.manifestsLoaded = {};
    ReadManifest(url, EXPECTED_PASS, aFilter);
}

// Note: If you materially change the reftest manifest parsing,
// please keep the parser in print-manifest-dirs.py in sync.
function ReadManifest(aURL, inherited_status, aFilter)
{
    // Ensure each manifest is only read once. This assumes that manifests that are
    // included with an unusual inherited_status or filters will be read via their
    // include before they are read directly in the case of a duplicate
    if (g.manifestsLoaded.hasOwnProperty(aURL.spec)) {
        if (g.manifestsLoaded[aURL.spec] === null)
            return;
        else
            aFilter = [aFilter[0], aFilter[1], true];
    }
    g.manifestsLoaded[aURL.spec] = aFilter[1];

    var secMan = CC[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                     .getService(CI.nsIScriptSecurityManager);

    var listURL = aURL;
    var channel = NetUtil.newChannel({uri: aURL, loadUsingSystemPrincipal: true});
    var inputStream = channel.open2();
    if (channel instanceof Components.interfaces.nsIHttpChannel
        && channel.responseStatus != 200) {
      g.logger.error("HTTP ERROR : " + channel.responseStatus);
    }
    var streamBuf = getStreamContent(inputStream);
    inputStream.close();
    var lines = streamBuf.split(/\n|\r|\r\n/);

    // Build the sandbox for fails-if(), etc., condition evaluation.
    var sandbox = BuildConditionSandbox(aURL);
    var lineNo = 0;
    var urlprefix = "";
    var defaultTestPrefSettings = [], defaultRefPrefSettings = [];
    if (g.compareRetainedDisplayLists) {
        AddRetainedDisplayListTestPrefs(sandbox, defaultTestPrefSettings,
                                        defaultRefPrefSettings);
    }
    if (g.compareStyloToGecko) {
        AddStyloTestPrefs(sandbox, defaultTestPrefSettings,
                          defaultRefPrefSettings);
    }
    for (var str of lines) {
        ++lineNo;
        if (str.charAt(0) == "#")
            continue; // entire line was a comment
        var i = str.search(/\s+#/);
        if (i >= 0)
            str = str.substring(0, i);
        // strip leading and trailing whitespace
        str = str.replace(/^\s*/, '').replace(/\s*$/, '');
        if (!str || str == "")
            continue;
        var items = str.split(/\s+/); // split on whitespace

        if (items[0] == "url-prefix") {
            if (items.length != 2)
                throw "url-prefix requires one url in manifest file " + aURL.spec + " line " + lineNo;
            urlprefix = items[1];
            continue;
        }

        if (items[0] == "default-preferences") {
            var m;
            var item;
            defaultTestPrefSettings = [];
            defaultRefPrefSettings = [];
            items.shift();
            while ((item = items.shift())) {
                if (!(m = item.match(RE_PREF_ITEM))) {
                    throw "Unexpected item in default-preferences list in manifest file " + aURL.spec + " line " + lineNo;
                }
                if (!AddPrefSettings(m[1], m[2], m[3], sandbox, defaultTestPrefSettings, defaultRefPrefSettings)) {
                    throw "Error in pref value in manifest file " + aURL.spec + " line " + lineNo;
                }
            }
            if (g.compareRetainedDisplayLists) {
                AddRetainedDisplayListTestPrefs(sandbox, defaultTestPrefSettings,
                                                defaultRefPrefSettings);
            }
            if (g.compareStyloToGecko) {
                AddStyloTestPrefs(sandbox, defaultTestPrefSettings,
                                  defaultRefPrefSettings);
            }
            continue;
        }

        var expected_status = EXPECTED_PASS;
        var allow_silent_fail = false;
        var minAsserts = 0;
        var maxAsserts = 0;
        var needs_focus = false;
        var slow = false;
        var testPrefSettings = defaultTestPrefSettings.concat();
        var refPrefSettings = defaultRefPrefSettings.concat();
        var fuzzy_delta = { min: 0, max: 2 };
        var fuzzy_pixels = { min: 0, max: 1 };
        var chaosMode = false;

        while (items[0].match(/^(fails|needs-focus|random|skip|asserts|slow|require-or|silentfail|pref|test-pref|ref-pref|fuzzy|chaos-mode)/)) {
            var item = items.shift();
            var stat;
            var cond;
            var m = item.match(/^(fails|random|skip|silentfail)-if(\(.*\))$/);
            if (m) {
                stat = m[1];
                // Note: m[2] contains the parentheses, and we want them.
                cond = Components.utils.evalInSandbox(m[2], sandbox);
            } else if (item.match(/^(fails|random|skip)$/)) {
                stat = item;
                cond = true;
            } else if (item == "needs-focus") {
                needs_focus = true;
                cond = false;
            } else if ((m = item.match(/^asserts\((\d+)(-\d+)?\)$/))) {
                cond = false;
                minAsserts = Number(m[1]);
                maxAsserts = (m[2] == undefined) ? minAsserts
                                                 : Number(m[2].substring(1));
            } else if ((m = item.match(/^asserts-if\((.*?),(\d+)(-\d+)?\)$/))) {
                cond = false;
                if (Components.utils.evalInSandbox("(" + m[1] + ")", sandbox)) {
                    minAsserts = Number(m[2]);
                    maxAsserts =
                      (m[3] == undefined) ? minAsserts
                                          : Number(m[3].substring(1));
                }
            } else if (item == "slow") {
                cond = false;
                slow = true;
            } else if ((m = item.match(/^require-or\((.*?)\)$/))) {
                var args = m[1].split(/,/);
                if (args.length != 2) {
                    throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": wrong number of args to require-or";
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
                if (Components.utils.evalInSandbox("(" + m[1] + ")", sandbox))
                    slow = true;
            } else if (item == "silentfail") {
                cond = false;
                allow_silent_fail = true;
            } else if ((m = item.match(RE_PREF_ITEM))) {
                cond = false;
                if (!AddPrefSettings(m[1], m[2], m[3], sandbox, testPrefSettings, refPrefSettings)) {
                    throw "Error in pref value in manifest file " + aURL.spec + " line " + lineNo;
                }
            } else if ((m = item.match(/^fuzzy\((\d+)(-\d+)?,(\d+)(-\d+)?\)$/))) {
              cond = false;
              expected_status = EXPECTED_FUZZY;
              fuzzy_delta = ExtractRange(m, 1);
              fuzzy_pixels = ExtractRange(m, 3);
            } else if ((m = item.match(/^fuzzy-if\((.*?),(\d+)(-\d+)?,(\d+)(-\d+)?\)$/))) {
              cond = false;
              if (Components.utils.evalInSandbox("(" + m[1] + ")", sandbox)) {
                expected_status = EXPECTED_FUZZY;
                fuzzy_delta = ExtractRange(m, 2);
                fuzzy_pixels = ExtractRange(m, 4);
              }
            } else if (item == "chaos-mode") {
                cond = false;
                chaosMode = true;
            } else {
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": unexpected item " + item;
            }

            if (cond) {
                if (stat == "fails") {
                    expected_status = EXPECTED_FAIL;
                } else if (stat == "random") {
                    expected_status = EXPECTED_RANDOM;
                } else if (stat == "skip") {
                    expected_status = EXPECTED_DEATH;
                } else if (stat == "silentfail") {
                    allow_silent_fail = true;
                }
            }
        }

        expected_status = Math.max(expected_status, inherited_status);

        if (minAsserts > maxAsserts) {
            throw "Bad range in manifest file " + aURL.spec + " line " + lineNo;
        }

        var runHttp = false;
        var httpDepth;
        if (items[0] == "HTTP") {
            runHttp = (aURL.scheme == "file"); // We can't yet run the local HTTP server
                                               // for non-local reftests.
            httpDepth = 0;
            items.shift();
        } else if (items[0].match(/HTTP\(\.\.(\/\.\.)*\)/)) {
            // Accept HTTP(..), HTTP(../..), HTTP(../../..), etc.
            runHttp = (aURL.scheme == "file"); // We can't yet run the local HTTP server
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

        var principal = secMan.createCodebasePrincipal(aURL, {});

        if (items[0] == "include") {
            if (items.length != 2)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect number of arguments to include";
            if (runHttp)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": use of include with http";
            var incURI = g.ioService.newURI(items[1], null, listURL);
            secMan.checkLoadURIWithPrincipal(principal, incURI,
                                             CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            ReadManifest(incURI, expected_status, aFilter);
        } else if (items[0] == TYPE_LOAD) {
            if (items.length != 2)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect number of arguments to load";
            if (expected_status != EXPECTED_PASS &&
                expected_status != EXPECTED_DEATH)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect known failure type for load test";
            var [testURI] = runHttp
                            ? ServeFiles(principal, httpDepth,
                                         listURL, [items[1]])
                            : [g.ioService.newURI(items[1], null, listURL)];
            var prettyPath = runHttp
                           ? g.ioService.newURI(items[1], null, listURL).spec
                           : testURI.spec;
            secMan.checkLoadURIWithPrincipal(principal, testURI,
                                             CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            AddTestItem({ type: TYPE_LOAD,
                          expected: expected_status,
                          allowSilentFail: allow_silent_fail,
                          prettyPath: prettyPath,
                          minAsserts: minAsserts,
                          maxAsserts: maxAsserts,
                          needsFocus: needs_focus,
                          slow: slow,
                          prefSettings1: testPrefSettings,
                          prefSettings2: refPrefSettings,
                          fuzzyMinDelta: fuzzy_delta.min,
                          fuzzyMaxDelta: fuzzy_delta.max,
                          fuzzyMinPixels: fuzzy_pixels.min,
                          fuzzyMaxPixels: fuzzy_pixels.max,
                          url1: testURI,
                          url2: null,
                          chaosMode: chaosMode }, aFilter);
        } else if (items[0] == TYPE_SCRIPT) {
            if (items.length != 2)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect number of arguments to script";
            var [testURI] = runHttp
                            ? ServeFiles(principal, httpDepth,
                                         listURL, [items[1]])
                            : [g.ioService.newURI(items[1], null, listURL)];
            var prettyPath = runHttp
                           ? g.ioService.newURI(items[1], null, listURL).spec
                           : testURI.spec;
            secMan.checkLoadURIWithPrincipal(principal, testURI,
                                             CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            AddTestItem({ type: TYPE_SCRIPT,
                          expected: expected_status,
                          allowSilentFail: allow_silent_fail,
                          prettyPath: prettyPath,
                          minAsserts: minAsserts,
                          maxAsserts: maxAsserts,
                          needsFocus: needs_focus,
                          slow: slow,
                          prefSettings1: testPrefSettings,
                          prefSettings2: refPrefSettings,
                          fuzzyMinDelta: fuzzy_delta.min,
                          fuzzyMaxDelta: fuzzy_delta.max,
                          fuzzyMinPixels: fuzzy_pixels.min,
                          fuzzyMaxPixels: fuzzy_pixels.max,
                          url1: testURI,
                          url2: null,
                          chaosMode: chaosMode }, aFilter);
        } else if (items[0] == TYPE_REFTEST_EQUAL || items[0] == TYPE_REFTEST_NOTEQUAL || items[0] == TYPE_PRINT) {
            if (items.length != 3)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect number of arguments to " + items[0];

            if (items[0] == TYPE_REFTEST_NOTEQUAL &&
                expected_status == EXPECTED_FUZZY &&
                (fuzzy_delta.min > 0 || fuzzy_pixels.min > 0)) {
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": minimum fuzz must be zero for tests of type " + items[0];
            }

            var [testURI, refURI] = runHttp
                                  ? ServeFiles(principal, httpDepth,
                                               listURL, [items[1], items[2]])
                                  : [g.ioService.newURI(items[1], null, listURL),
                                     g.ioService.newURI(items[2], null, listURL)];
            var prettyPath = runHttp
                           ? g.ioService.newURI(items[1], null, listURL).spec
                           : testURI.spec;
            secMan.checkLoadURIWithPrincipal(principal, testURI,
                                             CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            secMan.checkLoadURIWithPrincipal(principal, refURI,
                                             CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            var type = items[0];
            if (g.compareStyloToGecko || g.compareRetainedDisplayLists) {
                type = TYPE_REFTEST_EQUAL;
                refURI = testURI;

                // We expect twice as many assertion failures when running in
                // styloVsGecko mode because we run each test twice: once in
                // Stylo mode and once in Gecko mode.
                minAsserts *= 2;
                maxAsserts *= 2;

                // Skip the test if it is expected to fail in both Stylo and
                // Gecko modes. It would unexpectedly "pass" in styloVsGecko
                // mode when comparing the two failures, which is not a useful
                // result.
                if (expected_status === EXPECTED_FAIL ||
                    expected_status === EXPECTED_RANDOM) {
                    expected_status = EXPECTED_DEATH;
                }
            }

            AddTestItem({ type: type,
                          expected: expected_status,
                          allowSilentFail: allow_silent_fail,
                          prettyPath: prettyPath,
                          minAsserts: minAsserts,
                          maxAsserts: maxAsserts,
                          needsFocus: needs_focus,
                          slow: slow,
                          prefSettings1: testPrefSettings,
                          prefSettings2: refPrefSettings,
                          fuzzyMinDelta: fuzzy_delta.min,
                          fuzzyMaxDelta: fuzzy_delta.max,
                          fuzzyMinPixels: fuzzy_pixels.min,
                          fuzzyMaxPixels: fuzzy_pixels.max,
                          url1: testURI,
                          url2: refURI,
                          chaosMode: chaosMode }, aFilter);
        } else {
            throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": unknown test type " + items[0];
        }
    }
}

// Read all available data from an input stream and return it
// as a string.
function getStreamContent(inputStream)
{
    var streamBuf = "";
    var sis = CC["@mozilla.org/scriptableinputstream;1"].
                  createInstance(CI.nsIScriptableInputStream);
    sis.init(inputStream);

    var available;
    while ((available = sis.available()) != 0) {
        streamBuf += sis.read(available);
    }

    return streamBuf;
}

// Build the sandbox for fails-if(), etc., condition evaluation.
function BuildConditionSandbox(aURL) {
    var sandbox = new Components.utils.Sandbox(aURL.spec);
    var xr = CC[NS_XREAPPINFO_CONTRACTID].getService(CI.nsIXULRuntime);
    var appInfo = CC[NS_XREAPPINFO_CONTRACTID].getService(CI.nsIXULAppInfo);
    sandbox.isDebugBuild = g.debug.isDebugBuild;
    var prefs = CC["@mozilla.org/preferences-service;1"].
                getService(CI.nsIPrefBranch);
    var env = CC["@mozilla.org/process/environment;1"].
                getService(CI.nsIEnvironment);

    sandbox.xulRuntime = CU.cloneInto({widgetToolkit: xr.widgetToolkit, OS: xr.OS, XPCOMABI: xr.XPCOMABI}, sandbox);

    var testRect = g.browser.getBoundingClientRect();
    sandbox.smallScreen = false;
    if (g.containingWindow.innerWidth < 800 || g.containingWindow.innerHeight < 1000) {
        sandbox.smallScreen = true;
    }

    var gfxInfo = (NS_GFXINFO_CONTRACTID in CC) && CC[NS_GFXINFO_CONTRACTID].getService(CI.nsIGfxInfo);
    let readGfxInfo = function (obj, key) {
      if (g.contentGfxInfo && (key in g.contentGfxInfo)) {
        return g.contentGfxInfo[key];
      }
      return obj[key];
    }

    try {
      sandbox.d2d = readGfxInfo(gfxInfo, "D2DEnabled");
      sandbox.dwrite = readGfxInfo(gfxInfo, "DWriteEnabled");
    } catch (e) {
      sandbox.d2d = false;
      sandbox.dwrite = false;
    }

    var info = gfxInfo.getInfo();
    var canvasBackend = readGfxInfo(info, "AzureCanvasBackend");
    var contentBackend = readGfxInfo(info, "AzureContentBackend");
    var canvasAccelerated = readGfxInfo(info, "AzureCanvasAccelerated");

    sandbox.gpuProcess = gfxInfo.usingGPUProcess;
    sandbox.azureCairo = canvasBackend == "cairo";
    sandbox.azureSkia = canvasBackend == "skia";
    sandbox.skiaContent = contentBackend == "skia";
    sandbox.azureSkiaGL = canvasAccelerated; // FIXME: assumes GL right now
    // true if we are using the same Azure backend for rendering canvas and content
    sandbox.contentSameGfxBackendAsCanvas = contentBackend == canvasBackend
                                            || (contentBackend == "none" && canvasBackend == "cairo");

    sandbox.layersGPUAccelerated =
      g.windowUtils.layerManagerType != "Basic";
    sandbox.d3d11 =
      g.windowUtils.layerManagerType == "Direct3D 11";
    sandbox.d3d9 =
      g.windowUtils.layerManagerType == "Direct3D 9";
    sandbox.layersOpenGL =
      g.windowUtils.layerManagerType == "OpenGL";
    sandbox.webrender =
      g.windowUtils.layerManagerType == "WebRender";
    sandbox.layersOMTC =
      g.windowUtils.layerManagerRemote == true;
    sandbox.advancedLayers =
      g.windowUtils.usingAdvancedLayers == true;
    sandbox.layerChecksEnabled = !sandbox.webrender;

    sandbox.retainedDisplayList =
      prefs.getBoolPref("layout.display-list.retain");

    // Shortcuts for widget toolkits.
    sandbox.Android = xr.OS == "Android";
    sandbox.cocoaWidget = xr.widgetToolkit == "cocoa";
    sandbox.gtkWidget = xr.widgetToolkit == "gtk3";
    sandbox.qtWidget = xr.widgetToolkit == "qt";
    sandbox.winWidget = xr.widgetToolkit == "windows";

    // Scrollbars that are semi-transparent. See bug 1169666.
    sandbox.transparentScrollbars = xr.widgetToolkit == "gtk3";

    if (sandbox.Android) {
        var sysInfo = CC["@mozilla.org/system-info;1"].getService(CI.nsIPropertyBag2);

        // This is currently used to distinguish Android 4.0.3 (SDK version 15)
        // and later from Android 2.x
        sandbox.AndroidVersion = sysInfo.getPropertyAsInt32("version");
    }

#if MOZ_ASAN
    sandbox.AddressSanitizer = true;
#else
    sandbox.AddressSanitizer = false;
#endif

#if MOZ_WEBRTC
    sandbox.webrtc = true;
#else
    sandbox.webrtc = false;
#endif

let retainedDisplayListsEnabled = prefs.getBoolPref("layout.display-list.retain", false);
sandbox.retainedDisplayLists = retainedDisplayListsEnabled && !g.compareRetainedDisplayLists;
sandbox.compareRetainedDisplayLists = g.compareRetainedDisplayLists;

#ifdef MOZ_STYLO
    let styloEnabled = false;
    // Perhaps a bit redundant in places, but this is easier to compare with the
    // the real check in `nsLayoutUtils.cpp` to ensure they test the same way.
    if (env.get("STYLO_FORCE_ENABLED")) {
        styloEnabled = true;
    } else if (env.get("STYLO_FORCE_DISABLED")) {
        styloEnabled = false;
    } else {
        styloEnabled = prefs.getBoolPref("layout.css.servo.enabled", false);
    }
    sandbox.stylo = styloEnabled && !g.compareStyloToGecko;
    sandbox.styloVsGecko = g.compareStyloToGecko;
#else
    sandbox.stylo = false;
    sandbox.styloVsGecko = false;
#endif

// Printing via Skia PDF is only supported on Mac for now.
#ifdef XP_MACOSX && MOZ_ENABLE_SKIA_PDF
    sandbox.skiaPdf = true;
#else
    sandbox.skiaPdf = false;
#endif

#ifdef RELEASE_OR_BETA
    sandbox.release_or_beta = true;
#else
    sandbox.release_or_beta = false;
#endif

    var hh = CC[NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX + "http"].
                 getService(CI.nsIHttpProtocolHandler);
    var httpProps = ["userAgent", "appName", "appVersion", "vendor",
                     "vendorSub", "product", "productSub", "platform",
                     "oscpu", "language", "misc"];
    sandbox.http = new sandbox.Object();
    httpProps.forEach((x) => sandbox.http[x] = hh[x]);

    // Set OSX to be the Mac OS X version, as an integer, or undefined
    // for other platforms.  The integer is formed by 100 times the
    // major version plus the minor version, so 1006 for 10.6, 1010 for
    // 10.10, etc.
    var osxmatch = /Mac OS X (\d+).(\d+)$/.exec(hh.oscpu);
    sandbox.OSX = osxmatch ? parseInt(osxmatch[1]) * 100 + parseInt(osxmatch[2]) : undefined;

    // see if we have the test plugin available,
    // and set a sandox prop accordingly
    sandbox.haveTestPlugin = !sandbox.Android && !!getTestPlugin("Test Plug-in");

    // Set a flag on sandbox if the windows default theme is active
    sandbox.windowsDefaultTheme = g.containingWindow.matchMedia("(-moz-windows-default-theme)").matches;

    try {
        sandbox.nativeThemePref = !prefs.getBoolPref("mozilla.widget.disable-native-theme");
    } catch (e) {
        sandbox.nativeThemePref = true;
    }
    sandbox.gpuProcessForceEnabled = prefs.getBoolPref("layers.gpu-process.force-enabled", false);

    sandbox.prefs = CU.cloneInto({
        getBoolPref: function(p) { return prefs.getBoolPref(p); },
        getIntPref:  function(p) { return prefs.getIntPref(p); }
    }, sandbox, { cloneFunctions: true });

    // Tests shouldn't care about this except for when they need to
    // crash the content process
    sandbox.browserIsRemote = g.browserIsRemote;

    try {
        sandbox.asyncPan = g.containingWindow.document.docShell.asyncPanZoomEnabled;
    } catch (e) {
        sandbox.asyncPan = false;
    }

    // Graphics features
    sandbox.usesRepeatResampling = sandbox.d2d;

    if (!g.dumpedConditionSandbox) {
        g.logger.info("Dumping JSON representation of sandbox");
        g.logger.info(JSON.stringify(CU.waiveXrays(sandbox)));
        g.dumpedConditionSandbox = true;
    }

    return sandbox;
}

function AddRetainedDisplayListTestPrefs(aSandbox, aTestPrefSettings,
                                         aRefPrefSettings) {
    AddPrefSettings("test-", "layout.display-list.retain", "true", aSandbox,
                    aTestPrefSettings, aRefPrefSettings);
    AddPrefSettings("ref-", "layout.display-list.retain", "false", aSandbox,
                    aTestPrefSettings, aRefPrefSettings);
}

function AddStyloTestPrefs(aSandbox, aTestPrefSettings, aRefPrefSettings) {
    AddPrefSettings("test-", "layout.css.servo.enabled", "true", aSandbox,
                    aTestPrefSettings, aRefPrefSettings);
    AddPrefSettings("ref-", "layout.css.servo.enabled", "false", aSandbox,
                    aTestPrefSettings, aRefPrefSettings);
}

function AddPrefSettings(aWhere, aPrefName, aPrefValExpression, aSandbox, aTestPrefSettings, aRefPrefSettings) {
    var prefVal = Components.utils.evalInSandbox("(" + aPrefValExpression + ")", aSandbox);
    var prefType;
    var valType = typeof(prefVal);
    if (valType == "boolean") {
        prefType = PREF_BOOLEAN;
    } else if (valType == "string") {
        prefType = PREF_STRING;
    } else if (valType == "number" && (parseInt(prefVal) == prefVal)) {
        prefType = PREF_INTEGER;
    } else {
        return false;
    }
    var setting = { name: aPrefName,
                    type: prefType,
                    value: prefVal };

    if ((g.compareStyloToGecko && aPrefName != "layout.css.servo.enabled") ||
        (g.compareRetainedDisplayLists && aPrefName != "layout.display-list.retain")) {
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

function ExtractRange(matches, startIndex, defaultMin = 0) {
    if (matches[startIndex + 1] === undefined) {
        return {
            min: defaultMin,
            max: Number(matches[startIndex])
        };
    }
    return {
        min: Number(matches[startIndex]),
        max: Number(matches[startIndex + 1].substring(1))
    };
}

function ServeFiles(manifestPrincipal, depth, aURL, files) {
    var listURL = aURL.QueryInterface(CI.nsIFileURL);
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

    var secMan = CC[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                     .getService(CI.nsIScriptSecurityManager);

    var testbase = g.ioService.newURI("http://localhost:" + g.httpServerPort +
                                     path + dirPath);

    // Give the testbase URI access to XUL and XBL
    Services.perms.add(testbase, "allowXULXBL", Services.perms.ALLOW_ACTION);

    function FileToURI(file)
    {
        // Only serve relative URIs via the HTTP server, not absolute
        // ones like about:blank.
        var testURI = g.ioService.newURI(file, null, testbase);

        // XXX necessary?  manifestURL guaranteed to be file, others always HTTP
        secMan.checkLoadURIWithPrincipal(manifestPrincipal, testURI,
                                         CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);

        return testURI;
    }

    return files.map(FileToURI);
}

function AddTestItem(aTest, aFilter) {
    if (!aFilter)
        aFilter = [null, [], false];

    var globalFilter = aFilter[0];
    var manifestFilter = aFilter[1];
    var invertManifest = aFilter[2];
    if ((globalFilter && !globalFilter.test(aTest.url1.spec)) ||
        (manifestFilter &&
         !(invertManifest ^ manifestFilter.test(aTest.url1.spec))))
        return;
    if (g.focusFilterMode == FOCUS_FILTER_NEEDS_FOCUS_TESTS &&
        !aTest.needsFocus)
        return;
    if (g.focusFilterMode == FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS &&
        aTest.needsFocus)
        return;

    if (aTest.url2 !== null)
        aTest.identifier = [aTest.prettyPath, aTest.type, aTest.url2.spec];
    else
        aTest.identifier = aTest.prettyPath;

    g.urls.push(aTest);
}
