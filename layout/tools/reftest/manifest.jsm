/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ReadTopManifest", "CreateUrls"];

Cu.import("resource://reftest/globals.jsm", this);
Cu.import("resource://reftest/reftest.jsm", this);
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

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
    ReadManifest(url, aFilter);
}

// Note: If you materially change the reftest manifest parsing,
// please keep the parser in print-manifest-dirs.py in sync.
function ReadManifest(aURL, aFilter)
{
    // Ensure each manifest is only read once. This assumes that manifests that
    // are included with filters will be read via their include before they are
    // read directly in the case of a duplicate
    if (g.manifestsLoaded.hasOwnProperty(aURL.spec)) {
        if (g.manifestsLoaded[aURL.spec] === null)
            return;
        else
            aFilter = [aFilter[0], aFilter[1], true];
    }
    g.manifestsLoaded[aURL.spec] = aFilter[1];

    var secMan = Cc[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                     .getService(Ci.nsIScriptSecurityManager);

    var listURL = aURL;
    var channel = NetUtil.newChannel({uri: aURL, loadUsingSystemPrincipal: true});
    var inputStream = channel.open();
    if (channel instanceof Ci.nsIHttpChannel
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
        var wrCapture = { test: false, ref: false };
        var nonSkipUsed = false;
        var noAutoFuzz = false;

        while (items[0].match(/^(fails|needs-focus|random|skip|asserts|slow|require-or|silentfail|pref|test-pref|ref-pref|fuzzy|chaos-mode|wr-capture|wr-capture-ref|noautofuzz)/)) {
            var item = items.shift();
            var stat;
            var cond;
            var m = item.match(/^(fails|random|skip|silentfail)-if(\(.*\))$/);
            if (m) {
                stat = m[1];
                // Note: m[2] contains the parentheses, and we want them.
                cond = Cu.evalInSandbox(m[2], sandbox);
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
                if (Cu.evalInSandbox("(" + m[1] + ")", sandbox)) {
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
                if (Cu.evalInSandbox("(" + m[1] + ")", sandbox))
                    slow = true;
            } else if (item == "silentfail") {
                cond = false;
                allow_silent_fail = true;
            } else if ((m = item.match(RE_PREF_ITEM))) {
                cond = false;
                if (!AddPrefSettings(m[1], m[2], m[3], sandbox, testPrefSettings, refPrefSettings)) {
                    throw "Error in pref value in manifest file " + aURL.spec + " line " + lineNo;
                }
            } else if ((m = item.match(/^fuzzy\((\d+)-(\d+),(\d+)-(\d+)\)$/))) {
              cond = false;
              expected_status = EXPECTED_FUZZY;
              fuzzy_delta = ExtractRange(m, 1);
              fuzzy_pixels = ExtractRange(m, 3);
            } else if ((m = item.match(/^fuzzy-if\((.*?),(\d+)-(\d+),(\d+)-(\d+)\)$/))) {
              cond = false;
              if (Cu.evalInSandbox("(" + m[1] + ")", sandbox)) {
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
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": unexpected item " + item;
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
                    expected_status = EXPECTED_DEATH;
                } else if (stat == "silentfail") {
                    allow_silent_fail = true;
                }
            }
        }

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

        var principal = secMan.createContentPrincipal(aURL, {});

        if (items[0] == "include") {
            if (items.length != 2)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect number of arguments to include";
            if (runHttp)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": use of include with http";

            // If the expected_status is EXPECTED_PASS (the default) then allow
            // the include. If it is EXPECTED_DEATH, that means there was a skip
            // or skip-if annotation (with a true condition) on this include
            // statement, so we should skip the include. Any other expected_status
            // is disallowed since it's nonintuitive as to what the intended
            // effect is.
            if (nonSkipUsed) {
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": include statement with annotation other than 'skip' or 'skip-if'";
            } else if (expected_status == EXPECTED_DEATH) {
                g.logger.info("Skipping included manifest at " + aURL.spec + " line " + lineNo + " due to matching skip condition");
            } else {
                // poor man's assertion
                if (expected_status != EXPECTED_PASS) {
                    throw "Error in manifest file parsing code: we should never get expected_status=" + expected_status + " when nonSkipUsed=false (from " + aURL.spec + " line " + lineNo + ")";
                }

                var incURI = g.ioService.newURI(items[1], null, listURL);
                secMan.checkLoadURIWithPrincipal(principal, incURI,
                                                 Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
                ReadManifest(incURI, aFilter);
            }
        } else if (items[0] == TYPE_LOAD || items[0] == TYPE_SCRIPT) {
            var type = items[0];
            if (items.length != 2)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect number of arguments to " + type;
            if (type == TYPE_LOAD && expected_status != EXPECTED_PASS && expected_status != EXPECTED_DEATH)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect known failure type for load test";
            AddTestItem({ type: type,
                          expected: expected_status,
                          manifest: aURL.spec,
                          allowSilentFail: allow_silent_fail,
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
                          runHttp: runHttp,
                          httpDepth: httpDepth,
                          url1: items[1],
                          url2: null,
                          chaosMode: chaosMode,
                          wrCapture: wrCapture,
                          noAutoFuzz: noAutoFuzz }, aFilter);
        } else if (items[0] == TYPE_REFTEST_EQUAL || items[0] == TYPE_REFTEST_NOTEQUAL || items[0] == TYPE_PRINT) {
            if (items.length != 3)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect number of arguments to " + items[0];

            if (items[0] == TYPE_REFTEST_NOTEQUAL &&
                expected_status == EXPECTED_FUZZY &&
                (fuzzy_delta.min > 0 || fuzzy_pixels.min > 0)) {
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": minimum fuzz must be zero for tests of type " + items[0];
            }

            var type = items[0];
            if (g.compareRetainedDisplayLists) {
                type = TYPE_REFTEST_EQUAL;

                // We expect twice as many assertion failures when comparing
                // tests because we run each test twice.
                minAsserts *= 2;
                maxAsserts *= 2;

                // Skip the test if it is expected to fail in both modes.
                // It would unexpectedly "pass" in comparison mode mode when
                // comparing the two failures, which is not a useful result.
                if (expected_status === EXPECTED_FAIL ||
                    expected_status === EXPECTED_RANDOM) {
                    expected_status = EXPECTED_DEATH;
                }
            }

            AddTestItem({ type: type,
                          expected: expected_status,
                          manifest: aURL.spec,
                          allowSilentFail: allow_silent_fail,
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
                          runHttp: runHttp,
                          httpDepth: httpDepth,
                          url1: items[1],
                          url2: items[2],
                          chaosMode: chaosMode,
                          wrCapture: wrCapture,
                          noAutoFuzz: noAutoFuzz }, aFilter);
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
    var sis = Cc["@mozilla.org/scriptableinputstream;1"].
                  createInstance(Ci.nsIScriptableInputStream);
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
    var xr = Cc[NS_XREAPPINFO_CONTRACTID].getService(Ci.nsIXULRuntime);
    var appInfo = Cc[NS_XREAPPINFO_CONTRACTID].getService(Ci.nsIXULAppInfo);
    sandbox.isDebugBuild = g.debug.isDebugBuild;
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    var env = Cc["@mozilla.org/process/environment;1"].
                getService(Ci.nsIEnvironment);

    sandbox.xulRuntime = Cu.cloneInto({widgetToolkit: xr.widgetToolkit, OS: xr.OS, XPCOMABI: xr.XPCOMABI}, sandbox);

    var testRect = g.browser.getBoundingClientRect();
    sandbox.smallScreen = false;
    if (g.containingWindow.innerWidth < 800 || g.containingWindow.innerHeight < 1000) {
        sandbox.smallScreen = true;
    }

    var gfxInfo = (NS_GFXINFO_CONTRACTID in Cc) && Cc[NS_GFXINFO_CONTRACTID].getService(Ci.nsIGfxInfo);
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

    sandbox.gpuProcess = gfxInfo.usingGPUProcess;
    sandbox.azureCairo = canvasBackend == "cairo";
    sandbox.azureSkia = canvasBackend == "skia";
    sandbox.skiaContent = contentBackend == "skia";
    sandbox.azureSkiaGL = false;
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

    sandbox.usesOverlayScrollbars = g.windowUtils.usesOverlayScrollbars;

    // Shortcuts for widget toolkits.
    sandbox.Android = xr.OS == "Android";
    sandbox.cocoaWidget = xr.widgetToolkit == "cocoa";
    sandbox.gtkWidget = xr.widgetToolkit == "gtk3";
    sandbox.qtWidget = xr.widgetToolkit == "qt";
    sandbox.winWidget = xr.widgetToolkit == "windows";

    sandbox.is64Bit = xr.is64Bit;

    // GeckoView is currently uniquely identified by "android + e10s" but
    // we might want to make this condition more precise in the future.
    sandbox.geckoview = (sandbox.Android && g.browserIsRemote);

    // Scrollbars that are semi-transparent. See bug 1169666.
    sandbox.transparentScrollbars = xr.widgetToolkit == "gtk3";

    if (sandbox.Android) {
        var sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);

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

    sandbox.skiaPdf = false;

#ifdef RELEASE_OR_BETA
    sandbox.release_or_beta = true;
#else
    sandbox.release_or_beta = false;
#endif

    var hh = Cc[NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX + "http"].
                 getService(Ci.nsIHttpProtocolHandler);
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
        sandbox.nativeThemePref = !prefs.getBoolPref("widget.disable-native-theme");
    } catch (e) {
        sandbox.nativeThemePref = true;
    }
    sandbox.gpuProcessForceEnabled = prefs.getBoolPref("layers.gpu-process.force-enabled", false);

    sandbox.prefs = Cu.cloneInto({
        getBoolPref: function(p) { return prefs.getBoolPref(p); },
        getIntPref:  function(p) { return prefs.getIntPref(p); }
    }, sandbox, { cloneFunctions: true });

    // Tests shouldn't care about this except for when they need to
    // crash the content process
    sandbox.browserIsRemote = g.browserIsRemote;

    try {
        sandbox.asyncPan = g.containingWindow.docShell.asyncPanZoomEnabled;
    } catch (e) {
        sandbox.asyncPan = false;
    }

    // Graphics features
    sandbox.usesRepeatResampling = sandbox.d2d;

    // Running in a test-verify session?
    sandbox.verify = prefs.getBoolPref("reftest.verify", false);

    // Running with a variant enabled?
    sandbox.fission = prefs.getBoolPref("fission.autostart", false);
    sandbox.serviceWorkerE10s = prefs.getBoolPref("dom.serviceWorkers.parent_intercept", false);

    if (!g.dumpedConditionSandbox) {
        g.logger.info("Dumping JSON representation of sandbox");
        g.logger.info(JSON.stringify(Cu.waiveXrays(sandbox)));
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

function AddPrefSettings(aWhere, aPrefName, aPrefValExpression, aSandbox, aTestPrefSettings, aRefPrefSettings) {
    var prefVal = Cu.evalInSandbox("(" + aPrefValExpression + ")", aSandbox);
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

    if (g.compareRetainedDisplayLists && aPrefName != "layout.display-list.retain") {
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
        max: Number(matches[startIndex + 1])
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

    var secMan = Cc[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                     .getService(Ci.nsIScriptSecurityManager);

    var testbase = g.ioService.newURI("http://localhost:" + g.httpServerPort +
                                     path + dirPath);

    // Give the testbase URI access to XUL and XBL
    Services.perms.add(testbase, "allowXULXBL", Services.perms.ALLOW_ACTION);
    return testbase;
}

function CreateUrls(test) {
    let secMan = Cc[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                    .getService(Ci.nsIScriptSecurityManager);

    let manifestURL = g.ioService.newURI(test.manifest);

    let testbase = manifestURL;
    if (test.runHttp)
        testbase = ServeTestBase(manifestURL, test.httpDepth)

    function FileToURI(file)
    {
        if (file === null)
            return file;

        var testURI = g.ioService.newURI(file, null, testbase);
        let isChrome = testURI.scheme == "chrome";
        let principal = isChrome ? secMan.getSystemPrincipal() :
                                   secMan.createContentPrincipal(manifestURL, {});
        secMan.checkLoadURIWithPrincipal(principal, testURI,
                                         Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
        return testURI;
    }

    let files = [test.url1, test.url2];
    [test.url1, test.url2] = files.map(FileToURI);

    return test;
}

function AddTestItem(aTest, aFilter) {
    if (!aFilter)
        aFilter = [null, [], false];

    var {url1, url2} = CreateUrls(Object.assign({}, aTest));

    var globalFilter = aFilter[0];
    var manifestFilter = aFilter[1];
    var invertManifest = aFilter[2];
    if (globalFilter && !globalFilter.test(url1.spec))
        return;
    if (manifestFilter && !(invertManifest ^ manifestFilter.test(url1.spec)))
        return;
    if (g.focusFilterMode == FOCUS_FILTER_NEEDS_FOCUS_TESTS &&
        !aTest.needsFocus)
        return;
    if (g.focusFilterMode == FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS &&
        aTest.needsFocus)
        return;

    if (url2 !== null)
        aTest.identifier = [url1.spec, aTest.type, url2.spec];
    else
        aTest.identifier = url1.spec;
    g.urls.push(aTest);
    // Periodically log progress to avoid no-output timeout on slow platforms.
    // No-output timeouts during manifest parsing have been a problem for
    // jsreftests on Android/debug. Any logging resets the no-output timer,
    // even debug logging which is normally not displayed.
    if ((g.urls.length % 5000) == 0)
        g.logger.debug(g.urls.length + " tests found...");
}
