/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["OnRefTestLoad", "OnRefTestUnload"];

var CC = Components.classes;
const CI = Components.interfaces;
const CR = Components.results;
const CU = Components.utils;

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const NS_LOCAL_FILE_CONTRACTID = "@mozilla.org/file/local;1";
const NS_GFXINFO_CONTRACTID = "@mozilla.org/gfx/info;1";
const IO_SERVICE_CONTRACTID = "@mozilla.org/network/io-service;1";
const DEBUG_CONTRACTID = "@mozilla.org/xpcom/debug;1";
const NS_LOCALFILEINPUTSTREAM_CONTRACTID =
          "@mozilla.org/network/file-input-stream;1";
const NS_SCRIPTSECURITYMANAGER_CONTRACTID =
          "@mozilla.org/scriptsecuritymanager;1";
const NS_REFTESTHELPER_CONTRACTID =
          "@mozilla.org/reftest-helper;1";
const NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX =
          "@mozilla.org/network/protocol;1?name=";
const NS_XREAPPINFO_CONTRACTID =
          "@mozilla.org/xre/app-info;1";
const NS_DIRECTORY_SERVICE_CONTRACTID =
          "@mozilla.org/file/directory_service;1";
const NS_OBSERVER_SERVICE_CONTRACTID =
          "@mozilla.org/observer-service;1";

CU.import("resource://gre/modules/FileUtils.jsm");
CU.import("chrome://reftest/content/httpd.jsm", this);
CU.import("chrome://reftest/content/StructuredLog.jsm", this);
CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://gre/modules/NetUtil.jsm");
CU.import('resource://gre/modules/XPCOMUtils.jsm');

XPCOMUtils.defineLazyGetter(this, "OS", function() {
    const { OS } = CU.import("resource://gre/modules/osfile.jsm");
    return OS;
});

XPCOMUtils.defineLazyGetter(this, "PDFJS", function() {
    const { require } = CU.import("resource://gre/modules/commonjs/toolkit/require.js", {});
    return {
        main: require('resource://pdf.js/build/pdf.js'),
        worker: require('resource://pdf.js/build/pdf.worker.js')
    };
});

var gLoadTimeout = 0;
var gTimeoutHook = null;
var gRemote = false;
var gIgnoreWindowSize = false;
var gShuffle = false;
var gRepeat = null;
var gRunUntilFailure = false;
var gCleanupPendingCrashes = false;
var gTotalChunks = 0;
var gThisChunk = 0;
var gContainingWindow = null;
var gURLFilterRegex = {};
var gContentGfxInfo = null;
const FOCUS_FILTER_ALL_TESTS = "all";
const FOCUS_FILTER_NEEDS_FOCUS_TESTS = "needs-focus";
const FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS = "non-needs-focus";
var gFocusFilterMode = FOCUS_FILTER_ALL_TESTS;
var gCompareStyloToGecko = false;

// "<!--CLEAR-->"
const BLANK_URL_FOR_CLEARING = "data:text/html;charset=UTF-8,%3C%21%2D%2DCLEAR%2D%2D%3E";

var gBrowser;
// Are we testing web content loaded in a separate process?
var gBrowserIsRemote;           // bool
// Are we using <iframe mozbrowser>?
var gBrowserIsIframe;           // bool
var gBrowserMessageManager;
var gCanvas1, gCanvas2;
// gCurrentCanvas is non-null between InitCurrentCanvasWithSnapshot and the next
// RecordResult.
var gCurrentCanvas = null;
var gURLs;
var gManifestsLoaded = {};
// Map from URI spec to the number of times it remains to be used
var gURIUseCounts;
// Map from URI spec to the canvas rendered for that URI
var gURICanvases;
var gTestResults = {
  // Successful...
  Pass: 0,
  LoadOnly: 0,
  // Unexpected...
  Exception: 0,
  FailedLoad: 0,
  UnexpectedFail: 0,
  UnexpectedPass: 0,
  AssertionUnexpected: 0,
  AssertionUnexpectedFixed: 0,
  // Known problems...
  KnownFail : 0,
  AssertionKnown: 0,
  Random : 0,
  Skip: 0,
  Slow: 0,
};
var gTotalTests = 0;
var gState;
var gCurrentURL;
var gTestLog = [];
var gLogLevel;
var gServer;
var gCount = 0;
var gAssertionCount = 0;

var gIOService;
var gDebug;
var gWindowUtils;

var gSlowestTestTime = 0;
var gSlowestTestURL;
var gFailedUseWidgetLayers = false;

var gDrawWindowFlags;

var gExpectingProcessCrash = false;
var gExpectedCrashDumpFiles = [];
var gUnexpectedCrashDumpFiles = { };
var gCrashDumpDir;
var gPendingCrashDumpDir;
var gFailedNoPaint = false;
var gFailedOpaqueLayer = false;
var gFailedOpaqueLayerMessages = [];
var gFailedAssignedLayer = false;
var gFailedAssignedLayerMessages = [];

var gStartAfter = undefined;

// The enabled-state of the test-plugins, stored so they can be reset later
var gTestPluginEnabledStates = null;

const TYPE_REFTEST_EQUAL = '==';
const TYPE_REFTEST_NOTEQUAL = '!=';
const TYPE_LOAD = 'load';     // test without a reference (just test that it does
                              // not assert, crash, hang, or leak)
const TYPE_SCRIPT = 'script'; // test contains individual test results
const TYPE_PRINT = 'print'; // test and reference will be printed to PDF's and
                            // compared structurally

// The order of these constants matters, since when we have a status
// listed for a *manifest*, we combine the status with the status for
// the test by using the *larger*.
// FIXME: In the future, we may also want to use this rule for combining
// statuses that are on the same line (rather than making the last one
// win).
const EXPECTED_PASS = 0;
const EXPECTED_FAIL = 1;
const EXPECTED_RANDOM = 2;
const EXPECTED_DEATH = 3;  // test must be skipped to avoid e.g. crash/hang
const EXPECTED_FUZZY = 4;

// types of preference value we might want to set for a specific test
const PREF_BOOLEAN = 0;
const PREF_STRING  = 1;
const PREF_INTEGER = 2;

var gPrefsToRestore = [];

const gProtocolRE = /^\w+:/;
const gPrefItemRE = /^(|test-|ref-)pref\((.+?),(.*)\)$/;

var gHttpServerPort = -1;

// whether to run slow tests or not
var gRunSlowTests = true;

// whether we should skip caching canvases
var gNoCanvasCache = false;

var gRecycledCanvases = new Array();

// Only dump the sandbox once, because it doesn't depend on the
// manifest URL (yet!).
var gDumpedConditionSandbox = false;

var gTestPrintOutput = null;

function HasUnexpectedResult()
{
    return gTestResults.Exception > 0 ||
           gTestResults.FailedLoad > 0 ||
           gTestResults.UnexpectedFail > 0 ||
           gTestResults.UnexpectedPass > 0 ||
           gTestResults.AssertionUnexpected > 0 ||
           gTestResults.AssertionUnexpectedFixed > 0;
}

// By default we just log to stdout
var gLogFile = null;
var gDumpFn = function(line) {
  dump(line);
  if (gLogFile) {
    gLogFile.write(line, line.length);
  }
}
var gDumpRawLog = function(record) {
  // Dump JSON representation of data on a single line
  var line = "\n" + JSON.stringify(record) + "\n";
  dump(line);

  if (gLogFile) {
    gLogFile.write(line, line.length);
  }
}
var logger = new StructuredLogger('reftest', gDumpRawLog);

function TestBuffer(str)
{
  logger.debug(str);
  gTestLog.push(str);
}

function FlushTestBuffer()
{
  // In debug mode, we've dumped all these messages already.
  if (gLogLevel !== 'debug') {
    for (var i = 0; i < gTestLog.length; ++i) {
      logger.info("Saved log: " + gTestLog[i]);
    }
  }
  gTestLog = [];
}

function LogWidgetLayersFailure()
{
  logger.error("USE_WIDGET_LAYERS disabled because the screen resolution is too low. This falls back to an alternate rendering path (that may not be representative) and is not implemented with e10s enabled.");
  logger.error("Consider increasing your screen resolution, or adding '--disable-e10s' to your './mach reftest' command");
}

function AllocateCanvas()
{
    if (gRecycledCanvases.length > 0) {
        return gRecycledCanvases.shift();
    }

    var canvas = gContainingWindow.document.createElementNS(XHTML_NS, "canvas");
    var r = gBrowser.getBoundingClientRect();
    canvas.setAttribute("width", Math.ceil(r.width));
    canvas.setAttribute("height", Math.ceil(r.height));

    return canvas;
}

function ReleaseCanvas(canvas)
{
    // store a maximum of 2 canvases, if we're not caching
    if (!gNoCanvasCache || gRecycledCanvases.length < 2) {
        gRecycledCanvases.push(canvas);
    }
}

function IDForEventTarget(event)
{
    try {
        return "'" + event.target.getAttribute('id') + "'";
    } catch (ex) {
        return "<unknown>";
    }
}

function getTestPlugin(aName) {
  var ph = CC["@mozilla.org/plugin/host;1"].getService(CI.nsIPluginHost);
  var tags = ph.getPluginTags();

  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == aName)
      return tags[i];
  }

  logger.warning("Failed to find the test-plugin.");
  return null;
}

this.OnRefTestLoad = function OnRefTestLoad(win)
{
    gCrashDumpDir = CC[NS_DIRECTORY_SERVICE_CONTRACTID]
                    .getService(CI.nsIProperties)
                    .get("ProfD", CI.nsIFile);
    gCrashDumpDir.append("minidumps");

    gPendingCrashDumpDir = CC[NS_DIRECTORY_SERVICE_CONTRACTID]
                    .getService(CI.nsIProperties)
                    .get("UAppData", CI.nsIFile);
    gPendingCrashDumpDir.append("Crash Reports");
    gPendingCrashDumpDir.append("pending");

    var env = CC["@mozilla.org/process/environment;1"].
              getService(CI.nsIEnvironment);

    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);
    gBrowserIsRemote = prefs.getBoolPref("browser.tabs.remote.autostart", false);

    gBrowserIsIframe = prefs.getBoolPref("reftest.browser.iframe.enabled", false);

    gLogLevel = prefs.getCharPref("reftest.logLevel", "info");

    if (win === undefined || win == null) {
      win = window;
    }
    if (gContainingWindow == null && win != null) {
      gContainingWindow = win;
    }

    if (gBrowserIsIframe) {
      gBrowser = gContainingWindow.document.createElementNS(XHTML_NS, "iframe");
      gBrowser.setAttribute("mozbrowser", "");
    } else {
      gBrowser = gContainingWindow.document.createElementNS(XUL_NS, "xul:browser");
      gBrowser.setAttribute("class", "lightweight");
    }
    gBrowser.setAttribute("id", "browser");
    gBrowser.setAttribute("type", "content");
    gBrowser.setAttribute("primary", "true");
    gBrowser.setAttribute("remote", gBrowserIsRemote ? "true" : "false");
    // Make sure the browser element is exactly 800x1000, no matter
    // what size our window is
    gBrowser.setAttribute("style", "padding: 0px; margin: 0px; border:none; min-width: 800px; min-height: 1000px; max-width: 800px; max-height: 1000px");

    if (Services.appinfo.OS == "Android") {
      let doc = gContainingWindow.document.getElementById('main-window');
      while (doc.hasChildNodes()) {
        doc.firstChild.remove();
      }
      doc.appendChild(gBrowser);
    } else {
      document.getElementById("reftest-window").appendChild(gBrowser);
    }

    // reftests should have the test plugins enabled, not click-to-play
    let plugin1 = getTestPlugin("Test Plug-in");
    let plugin2 = getTestPlugin("Second Test Plug-in");
    if (plugin1 && plugin2) {
      gTestPluginEnabledStates = [plugin1.enabledState, plugin2.enabledState];
      plugin1.enabledState = CI.nsIPluginTag.STATE_ENABLED;
      plugin2.enabledState = CI.nsIPluginTag.STATE_ENABLED;
    } else {
      logger.warning("Could not get test plugin tags.");
    }

    gBrowserMessageManager = gBrowser.frameLoader.messageManager;
    // The content script waits for the initial onload, then notifies
    // us.
    RegisterMessageListenersAndLoadContentScript();
}

function InitAndStartRefTests()
{
    /* These prefs are optional, so we don't need to spit an error to the log */
    try {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                    getService(Components.interfaces.nsIPrefBranch);
    } catch(e) {
        logger.error("EXCEPTION: " + e);
    }

    try {
      prefs.setBoolPref("android.widget_paints_background", false);
    } catch (e) {}

    /* set the gLoadTimeout */
    try {
        gLoadTimeout = prefs.getIntPref("reftest.timeout");
    } catch(e) {
        gLoadTimeout = 5 * 60 * 1000; //5 minutes as per bug 479518
    }

    /* Get the logfile for android tests */
    try {
        var logFile = prefs.getCharPref("reftest.logFile");
        if (logFile) {
            var f = FileUtils.File(logFile);
            gLogFile = FileUtils.openFileOutputStream(f, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE);
        }
    } catch(e) {}

    gRemote = prefs.getBoolPref("reftest.remote", false);

    gIgnoreWindowSize = prefs.getBoolPref("reftest.ignoreWindowSize", false);

    /* Support for running a chunk (subset) of tests.  In separate try as this is optional */
    try {
        gTotalChunks = prefs.getIntPref("reftest.totalChunks");
        gThisChunk = prefs.getIntPref("reftest.thisChunk");
    }
    catch(e) {
        gTotalChunks = 0;
        gThisChunk = 0;
    }

    try {
        gFocusFilterMode = prefs.getCharPref("reftest.focusFilterMode");
    } catch(e) {}

    try {
        gStartAfter = prefs.getCharPref("reftest.startAfter");
    } catch(e) {
        gStartAfter = undefined;
    }

#ifdef MOZ_STYLO
    try {
        gCompareStyloToGecko = prefs.getBoolPref("reftest.compareStyloToGecko");
    } catch(e) {}
#endif

#ifdef MOZ_ENABLE_SKIA_PDF
    try {
        // We have to disable printing via parent or else silent print operations
        // (the type that we use here) would be treated as non-silent -- in other
        // words, a print dialog would appear for each print operation, which
        // would interrupt the test run.
        // See http://searchfox.org/mozilla-central/rev/bd39b6170f04afeefc751a23bb04e18bbd10352b/layout/printing/nsPrintEngine.cpp#617
        prefs.setBoolPref("print.print_via_parent", false);
    } catch (e) {
        /* uh oh, print reftests may not work... */
    }
#endif

    gWindowUtils = gContainingWindow.QueryInterface(CI.nsIInterfaceRequestor).getInterface(CI.nsIDOMWindowUtils);
    if (!gWindowUtils || !gWindowUtils.compareCanvases)
        throw "nsIDOMWindowUtils inteface missing";

    gIOService = CC[IO_SERVICE_CONTRACTID].getService(CI.nsIIOService);
    gDebug = CC[DEBUG_CONTRACTID].getService(CI.nsIDebug2);

    RegisterProcessCrashObservers();

    if (gRemote) {
        gServer = null;
    } else {
        gServer = new HttpServer();
    }
    try {
        if (gServer)
            StartHTTPServer();
    } catch (ex) {
        //gBrowser.loadURI('data:text/plain,' + ex);
        ++gTestResults.Exception;
        logger.error("EXCEPTION: " + ex);
        DoneTests();
    }

    // Focus the content browser.
    if (gFocusFilterMode != FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS) {
        gBrowser.addEventListener("focus", StartTests, true);
        gBrowser.focus();
    } else {
        StartTests();
    }
}

function StartHTTPServer()
{
    gServer.registerContentType("sjs", "sjs");
    gServer.start(-1);
    gHttpServerPort = gServer.identity.primaryPort;
}

// Perform a Fisher-Yates shuffle of the array.
function Shuffle(array)
{
    for (var i = array.length - 1; i > 0; i--) {
        var j = Math.floor(Math.random() * (i + 1));
        var temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

function StartTests()
{
    if (gFocusFilterMode != FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS) {
        gBrowser.removeEventListener("focus", StartTests, true);
    }

    var manifests;
    /* These prefs are optional, so we don't need to spit an error to the log */
    try {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                    getService(Components.interfaces.nsIPrefBranch);
    } catch(e) {
        logger.error("EXCEPTION: " + e);
    }

    gNoCanvasCache = prefs.getIntPref("reftest.nocache", false);

    gShuffle = prefs.getBoolPref("reftest.shuffle", false);

    gRunUntilFailure = prefs.getBoolPref("reftest.runUntilFailure", false);

    gCleanupPendingCrashes = prefs.getBoolPref("reftest.cleanupPendingCrashes", false);

    // Check if there are any crash dump files from the startup procedure, before
    // we start running the first test. Otherwise the first test might get
    // blamed for producing a crash dump file when that was not the case.
    CleanUpCrashDumpFiles();

    // When we repeat this function is called again, so really only want to set
    // gRepeat once.
    if (gRepeat == null) {
      gRepeat = prefs.getIntPref("reftest.repeat", 0);
    }

    gRunSlowTests = prefs.getIntPref("reftest.skipslowtests", false);

    if (gShuffle) {
        gNoCanvasCache = true;
    }

    gURLs = [];
    gManifestsLoaded = {};

    try {
        var manifests = JSON.parse(prefs.getCharPref("reftest.manifests"));
        gURLFilterRegex = manifests[null];
    } catch(e) {
        logger.error("Unable to find reftest.manifests pref.  Please ensure your profile is setup properly");
        DoneTests();
    }

    try {
        var globalFilter = manifests.hasOwnProperty("") ? new RegExp(manifests[""]) : null;
        var manifestURLs = Object.keys(manifests);

        // Ensure we read manifests from higher up the directory tree first so that we
        // process includes before reading the included manifest again
        manifestURLs.sort(function(a,b) {return a.length - b.length})
        manifestURLs.forEach(function(manifestURL) {
            logger.info("Reading manifest " + manifestURL);
            var filter = manifests[manifestURL] ? new RegExp(manifests[manifestURL]) : null;
            ReadTopManifest(manifestURL, [globalFilter, filter, false]);
        });
        BuildUseCounts();

        // Filter tests which will be skipped to get a more even distribution when chunking
        // tURLs is a temporary array containing all active tests
        var tURLs = new Array();
        var tIDs = new Array();
        for (var i = 0; i < gURLs.length; ++i) {
            if (gURLs[i].expected == EXPECTED_DEATH)
                continue;

            if (gURLs[i].needsFocus && !Focus())
                continue;

            if (gURLs[i].slow && !gRunSlowTests)
                continue;

            tURLs.push(gURLs[i]);
            tIDs.push(gURLs[i].identifier);
        }

        if (gStartAfter === undefined) {
            logger.suiteStart(tIDs, {"skipped": gURLs.length - tURLs.length});
        }

        if (gTotalChunks > 0 && gThisChunk > 0) {
            // Calculate start and end indices of this chunk if tURLs array were
            // divided evenly
            var testsPerChunk = tURLs.length / gTotalChunks;
            var start = Math.round((gThisChunk-1) * testsPerChunk);
            var end = Math.round(gThisChunk * testsPerChunk);

            // Map these indices onto the gURLs array. This avoids modifying the
            // gURLs array which prevents skipped tests from showing up in the log
            start = gThisChunk == 1 ? 0 : gURLs.indexOf(tURLs[start]);
            end = gThisChunk == gTotalChunks ? gURLs.length : gURLs.indexOf(tURLs[end + 1]) - 1;

            logger.info("Running chunk " + gThisChunk + " out of " + gTotalChunks + " chunks.  " +
                "tests " + (start+1) + "-" + end + "/" + gURLs.length);

            gURLs = gURLs.slice(start, end);
        }

        if (gShuffle) {
            if (gStartAfter !== undefined) {
                logger.error("Can't resume from a crashed test when " +
                             "--shuffle is enabled, continue by shuffling " +
                             "all the tests");
                DoneTests();
                return;
            }
            Shuffle(gURLs);
        } else if (gStartAfter !== undefined) {
            // Skip through previously crashed test
            // We have to do this after chunking so we don't break the numbers
            var crash_idx = gURLs.map(function(url) {
                return url['url1']['spec'];
            }).indexOf(gStartAfter);
            if (crash_idx == -1) {
                throw "Can't find the previously crashed test";
            }
            gURLs = gURLs.slice(crash_idx + 1);
        }

        gTotalTests = gURLs.length;

        if (!gTotalTests)
            throw "No tests to run";

        gURICanvases = {};
        StartCurrentTest();
    } catch (ex) {
        //gBrowser.loadURI('data:text/plain,' + ex);
        ++gTestResults.Exception;
        logger.error("EXCEPTION: " + ex);
        DoneTests();
    }
}

function OnRefTestUnload()
{
  let plugin1 = getTestPlugin("Test Plug-in");
  let plugin2 = getTestPlugin("Second Test Plug-in");
  if (plugin1 && plugin2) {
    plugin1.enabledState = gTestPluginEnabledStates[0];
    plugin2.enabledState = gTestPluginEnabledStates[1];
  } else {
    logger.warning("Failed to get test plugin tags.");
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
    sandbox.isDebugBuild = gDebug.isDebugBuild;
    var prefs = CC["@mozilla.org/preferences-service;1"].
                getService(CI.nsIPrefBranch);
    var env = CC["@mozilla.org/process/environment;1"].
                getService(CI.nsIEnvironment);

    // xr.XPCOMABI throws exception for configurations without full ABI
    // support (mobile builds on ARM)
    var XPCOMABI = "";
    try {
        XPCOMABI = xr.XPCOMABI;
    } catch(e) {}

    sandbox.xulRuntime = CU.cloneInto({widgetToolkit: xr.widgetToolkit, OS: xr.OS, XPCOMABI: XPCOMABI}, sandbox);

    var testRect = gBrowser.getBoundingClientRect();
    sandbox.smallScreen = false;
    if (gContainingWindow.innerWidth < 800 || gContainingWindow.innerHeight < 1000) {
        sandbox.smallScreen = true;
    }

    var gfxInfo = (NS_GFXINFO_CONTRACTID in CC) && CC[NS_GFXINFO_CONTRACTID].getService(CI.nsIGfxInfo);
    let readGfxInfo = function (obj, key) {
      if (gContentGfxInfo && (key in gContentGfxInfo)) {
        return gContentGfxInfo[key];
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
      gWindowUtils.layerManagerType != "Basic";
    sandbox.d3d11 =
      gWindowUtils.layerManagerType == "Direct3D 11";
    sandbox.d3d9 =
      gWindowUtils.layerManagerType == "Direct3D 9";
    sandbox.layersOpenGL =
      gWindowUtils.layerManagerType == "OpenGL";
    sandbox.webrender =
      gWindowUtils.layerManagerType == "WebRender";
    sandbox.layersOMTC =
      gWindowUtils.layerManagerRemote == true;
    sandbox.advancedLayers =
      gWindowUtils.usingAdvancedLayers == true;

    // Shortcuts for widget toolkits.
    sandbox.Android = xr.OS == "Android";
    sandbox.cocoaWidget = xr.widgetToolkit == "cocoa";
    sandbox.gtkWidget = xr.widgetToolkit == "gtk2"
                        || xr.widgetToolkit == "gtk3";
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
    sandbox.stylo = styloEnabled && !gCompareStyloToGecko;
    sandbox.styloVsGecko = gCompareStyloToGecko;
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
    sandbox.haveTestPlugin = !!getTestPlugin("Test Plug-in");

    // Set a flag on sandbox if the windows default theme is active
    sandbox.windowsDefaultTheme = gContainingWindow.matchMedia("(-moz-windows-default-theme)").matches;

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
    sandbox.browserIsRemote = gBrowserIsRemote;

    try {
        sandbox.asyncPan = gContainingWindow.document.docShell.asyncPanZoomEnabled;
    } catch (e) {
        sandbox.asyncPan = false;
    }

    // Graphics features
    sandbox.usesRepeatResampling = sandbox.d2d;

    if (!gDumpedConditionSandbox) {
        logger.info("Dumping JSON representation of sandbox");
        logger.info(JSON.stringify(CU.waiveXrays(sandbox)));
        gDumpedConditionSandbox = true;
    }

    return sandbox;
}

function AddPrefSettings(aWhere, aPrefName, aPrefValExpression, aSandbox, aTestPrefSettings, aRefPrefSettings)
{
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

    if (gCompareStyloToGecko && aPrefName != "layout.css.servo.enabled") {
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

function ReadTopManifest(aFileURL, aFilter)
{
    var url = gIOService.newURI(aFileURL);
    if (!url)
        throw "Expected a file or http URL for the manifest.";
    ReadManifest(url, EXPECTED_PASS, aFilter);
}

function AddTestItem(aTest, aFilter)
{
    if (!aFilter)
        aFilter = [null, [], false];

    var globalFilter = aFilter[0];
    var manifestFilter = aFilter[1];
    var invertManifest = aFilter[2];
    if ((globalFilter && !globalFilter.test(aTest.url1.spec)) ||
        (manifestFilter &&
         !(invertManifest ^ manifestFilter.test(aTest.url1.spec))))
        return;
    if (gFocusFilterMode == FOCUS_FILTER_NEEDS_FOCUS_TESTS &&
        !aTest.needsFocus)
        return;
    if (gFocusFilterMode == FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS &&
        aTest.needsFocus)
        return;

    if (aTest.url2 !== null)
        aTest.identifier = [aTest.prettyPath, aTest.type, aTest.url2.spec];
    else
        aTest.identifier = aTest.prettyPath;

    gURLs.push(aTest);
}

function AddStyloTestPrefs(aSandbox, aTestPrefSettings, aRefPrefSettings)
{
    AddPrefSettings("test-", "layout.css.servo.enabled", "true", aSandbox,
                    aTestPrefSettings, aRefPrefSettings);
    AddPrefSettings("ref-", "layout.css.servo.enabled", "false", aSandbox,
                    aTestPrefSettings, aRefPrefSettings);
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

// Note: If you materially change the reftest manifest parsing,
// please keep the parser in print-manifest-dirs.py in sync.
function ReadManifest(aURL, inherited_status, aFilter)
{
    // Ensure each manifest is only read once. This assumes that manifests that are
    // included with an unusual inherited_status or filters will be read via their
    // include before they are read directly in the case of a duplicate
    if (gManifestsLoaded.hasOwnProperty(aURL.spec)) {
        if (gManifestsLoaded[aURL.spec] === null)
            return;
        else
            aFilter = [aFilter[0], aFilter[1], true];
    }
    gManifestsLoaded[aURL.spec] = aFilter[1];

    var secMan = CC[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                     .getService(CI.nsIScriptSecurityManager);

    var listURL = aURL;
    var channel = NetUtil.newChannel({uri: aURL, loadUsingSystemPrincipal: true});
    var inputStream = channel.open2();
    if (channel instanceof Components.interfaces.nsIHttpChannel
        && channel.responseStatus != 200) {
      logger.error("HTTP ERROR : " + channel.responseStatus);
    }
    var streamBuf = getStreamContent(inputStream);
    inputStream.close();
    var lines = streamBuf.split(/\n|\r|\r\n/);

    // Build the sandbox for fails-if(), etc., condition evaluation.
    var sandbox = BuildConditionSandbox(aURL);
    var lineNo = 0;
    var urlprefix = "";
    var defaultTestPrefSettings = [], defaultRefPrefSettings = [];
    if (gCompareStyloToGecko) {
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
                if (!(m = item.match(gPrefItemRE))) {
                    throw "Unexpected item in default-preferences list in manifest file " + aURL.spec + " line " + lineNo;
                }
                if (!AddPrefSettings(m[1], m[2], m[3], sandbox, defaultTestPrefSettings, defaultRefPrefSettings)) {
                    throw "Error in pref value in manifest file " + aURL.spec + " line " + lineNo;
                }
            }
            if (gCompareStyloToGecko) {
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
            } else if ((m = item.match(gPrefItemRE))) {
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
            if (items.length > 1 && !items[1].match(gProtocolRE)) {
                items[1] = urlprefix + items[1];
            }
            if (items.length > 2 && !items[2].match(gProtocolRE)) {
                items[2] = urlprefix + items[2];
            }
        }

        var principal = secMan.createCodebasePrincipal(aURL, {});

        if (items[0] == "include") {
            if (items.length != 2)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": incorrect number of arguments to include";
            if (runHttp)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo + ": use of include with http";
            var incURI = gIOService.newURI(items[1], null, listURL);
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
                            : [gIOService.newURI(items[1], null, listURL)];
            var prettyPath = runHttp
                           ? gIOService.newURI(items[1], null, listURL).spec
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
                            : [gIOService.newURI(items[1], null, listURL)];
            var prettyPath = runHttp
                           ? gIOService.newURI(items[1], null, listURL).spec
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
                                  : [gIOService.newURI(items[1], null, listURL),
                                     gIOService.newURI(items[2], null, listURL)];
            var prettyPath = runHttp
                           ? gIOService.newURI(items[1], null, listURL).spec
                           : testURI.spec;
            secMan.checkLoadURIWithPrincipal(principal, testURI,
                                             CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            secMan.checkLoadURIWithPrincipal(principal, refURI,
                                             CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            var type = items[0];
            if (gCompareStyloToGecko) {
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

function AddURIUseCount(uri)
{
    if (uri == null)
        return;

    var spec = uri.spec;
    if (spec in gURIUseCounts) {
        gURIUseCounts[spec]++;
    } else {
        gURIUseCounts[spec] = 1;
    }
}

function BuildUseCounts()
{
    if (gNoCanvasCache) {
        return;
    }

    gURIUseCounts = {};
    for (var i = 0; i < gURLs.length; ++i) {
        var url = gURLs[i];
        if (url.expected != EXPECTED_DEATH &&
            (url.type == TYPE_REFTEST_EQUAL ||
             url.type == TYPE_REFTEST_NOTEQUAL)) {
            if (url.prefSettings1.length == 0) {
                AddURIUseCount(gURLs[i].url1);
            }
            if (url.prefSettings2.length == 0) {
                AddURIUseCount(gURLs[i].url2);
            }
        }
    }
}

function ServeFiles(manifestPrincipal, depth, aURL, files)
{
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

    gCount++;
    var path = "/" + Date.now() + "/" + gCount;
    gServer.registerDirectory(path + "/", directory);

    var secMan = CC[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                     .getService(CI.nsIScriptSecurityManager);

    var testbase = gIOService.newURI("http://localhost:" + gHttpServerPort +
                                     path + dirPath);

    // Give the testbase URI access to XUL and XBL
    Services.perms.add(testbase, "allowXULXBL", Services.perms.ALLOW_ACTION);

    function FileToURI(file)
    {
        // Only serve relative URIs via the HTTP server, not absolute
        // ones like about:blank.
        var testURI = gIOService.newURI(file, null, testbase);

        // XXX necessary?  manifestURL guaranteed to be file, others always HTTP
        secMan.checkLoadURIWithPrincipal(manifestPrincipal, testURI,
                                         CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);

        return testURI;
    }

    return files.map(FileToURI);
}

// Return true iff this window is focused when this function returns.
function Focus()
{
    var fm = CC["@mozilla.org/focus-manager;1"].getService(CI.nsIFocusManager);
    fm.focusedWindow = gContainingWindow;
#ifdef XP_MACOSX
    try {
        var dock = CC["@mozilla.org/widget/macdocksupport;1"].getService(CI.nsIMacDockSupport);
        dock.activateApplication(true);
    } catch(ex) {
    }
#endif // XP_MACOSX
    return true;
}

function Blur()
{
    // On non-remote reftests, this will transfer focus to the dummy window
    // we created to hold focus for non-needs-focus tests.  Buggy tests
    // (ones which require focus but don't request needs-focus) will then
    // fail.
    gContainingWindow.blur();
}

function StartCurrentTest()
{
    gTestLog = [];

    // make sure we don't run tests that are expected to kill the browser
    while (gURLs.length > 0) {
        var test = gURLs[0];
        logger.testStart(test.identifier);
        if (test.expected == EXPECTED_DEATH) {
            ++gTestResults.Skip;
            logger.testEnd(test.identifier, "SKIP");
            gURLs.shift();
        } else if (test.needsFocus && !Focus()) {
            // FIXME: Marking this as a known fail is dangerous!  What
            // if it starts failing all the time?
            ++gTestResults.Skip;
            logger.testEnd(test.identifier, "SKIP", null, "(COULDN'T GET FOCUS)");
            gURLs.shift();
        } else if (test.slow && !gRunSlowTests) {
            ++gTestResults.Slow;
            logger.testEnd(test.identifier, "SKIP", null, "(SLOW)");
            gURLs.shift();
        } else {
            break;
        }
    }

    if ((gURLs.length == 0 && gRepeat == 0) ||
        (gRunUntilFailure && HasUnexpectedResult())) {
        RestoreChangedPreferences();
        DoneTests();
    } else if (gURLs.length == 0 && gRepeat > 0) {
        // Repeat
        gRepeat--;
        StartTests();
    } else {
        if (gURLs[0].chaosMode) {
            gWindowUtils.enterChaosMode();
        }
        if (!gURLs[0].needsFocus) {
            Blur();
        }
        var currentTest = gTotalTests - gURLs.length;
        gContainingWindow.document.title = "reftest: " + currentTest + " / " + gTotalTests +
            " (" + Math.floor(100 * (currentTest / gTotalTests)) + "%)";
        StartCurrentURI(1);
    }
}

function StartCurrentURI(aState)
{
    gState = aState;
    gCurrentURL = gURLs[0]["url" + aState].spec;

    RestoreChangedPreferences();

    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
        getService(Components.interfaces.nsIPrefBranch);

    var prefSettings = gURLs[0]["prefSettings" + aState];
    if (prefSettings.length > 0) {
        var badPref = undefined;
        try {
            prefSettings.forEach(function(ps) {
                var oldVal;
                if (ps.type == PREF_BOOLEAN) {
                    try {
                        oldVal = prefs.getBoolPref(ps.name);
                    } catch (e) {
                        badPref = "boolean preference '" + ps.name + "'";
                        throw "bad pref";
                    }
                } else if (ps.type == PREF_STRING) {
                    try {
                        oldVal = prefs.getCharPref(ps.name);
                    } catch (e) {
                        badPref = "string preference '" + ps.name + "'";
                        throw "bad pref";
                    }
                } else if (ps.type == PREF_INTEGER) {
                    try {
                        oldVal = prefs.getIntPref(ps.name);
                    } catch (e) {
                        badPref = "integer preference '" + ps.name + "'";
                        throw "bad pref";
                    }
                } else {
                    throw "internal error - unknown preference type";
                }
                if (oldVal != ps.value) {
                    gPrefsToRestore.push( { name: ps.name,
                                            type: ps.type,
                                            value: oldVal } );
                    var value = ps.value;
                    if (ps.type == PREF_BOOLEAN) {
                        prefs.setBoolPref(ps.name, value);
                    } else if (ps.type == PREF_STRING) {
                        prefs.setCharPref(ps.name, value);
                        value = '"' + value + '"';
                    } else if (ps.type == PREF_INTEGER) {
                        prefs.setIntPref(ps.name, value);
                    }
                    logger.info("SET PREFERENCE pref(" + ps.name + "," + value + ")");
                }
            });
        } catch (e) {
            if (e == "bad pref") {
                var test = gURLs[0];
                if (test.expected == EXPECTED_FAIL) {
                    logger.testEnd(test.identifier, "FAIL", "FAIL",
                                   "(SKIPPED; " + badPref + " not known or wrong type)");
                    ++gTestResults.Skip;
                } else {
                    logger.testEnd(test.identifier, "FAIL", "PASS",
                                   badPref + " not known or wrong type");
                    ++gTestResults.UnexpectedFail;
                }

                // skip the test that had a bad preference
                gURLs.shift();
                StartCurrentTest();
                return;
            } else {
                throw e;
            }
        }
    }

    if (prefSettings.length == 0 &&
        gURICanvases[gCurrentURL] &&
        (gURLs[0].type == TYPE_REFTEST_EQUAL ||
         gURLs[0].type == TYPE_REFTEST_NOTEQUAL) &&
        gURLs[0].maxAsserts == 0) {
        // Pretend the document loaded --- RecordResult will notice
        // there's already a canvas for this URL
        gContainingWindow.setTimeout(RecordResult, 0);
    } else {
        var currentTest = gTotalTests - gURLs.length;
        // Log this to preserve the same overall log format,
        // should be removed if the format is updated
        gDumpFn("REFTEST TEST-LOAD | " + gCurrentURL + " | " + currentTest + " / " + gTotalTests +
                " (" + Math.floor(100 * (currentTest / gTotalTests)) + "%)\n");
        TestBuffer("START " + gCurrentURL);
        var type = gURLs[0].type
        if (TYPE_SCRIPT == type) {
            SendLoadScriptTest(gCurrentURL, gLoadTimeout);
        } else if (TYPE_PRINT == type) {
            SendLoadPrintTest(gCurrentURL, gLoadTimeout);
        } else {
            SendLoadTest(type, gCurrentURL, gLoadTimeout);
        }
    }
}

function DoneTests()
{
    logger.suiteEnd({'results': gTestResults});
    logger.info("Slowest test took " + gSlowestTestTime + "ms (" + gSlowestTestURL + ")");
    logger.info("Total canvas count = " + gRecycledCanvases.length);
    if (gFailedUseWidgetLayers) {
        LogWidgetLayersFailure();
    }

    function onStopped() {
        let appStartup = CC["@mozilla.org/toolkit/app-startup;1"].getService(CI.nsIAppStartup);
        appStartup.quit(CI.nsIAppStartup.eForceQuit);
    }
    if (gServer) {
        gServer.stop(onStopped);
    }
    else {
        onStopped();
    }
}

function UpdateCanvasCache(url, canvas)
{
    var spec = url.spec;

    --gURIUseCounts[spec];

    if (gURIUseCounts[spec] == 0) {
        ReleaseCanvas(canvas);
        delete gURICanvases[spec];
    } else if (gURIUseCounts[spec] > 0) {
        gURICanvases[spec] = canvas;
    } else {
        throw "Use counts were computed incorrectly";
    }
}

// Recompute drawWindow flags for every drawWindow operation.
// We have to do this every time since our window can be
// asynchronously resized (e.g. by the window manager, to make
// it fit on screen) at unpredictable times.
// Fortunately this is pretty cheap.
function DoDrawWindow(ctx, x, y, w, h)
{
    var flags = ctx.DRAWWINDOW_DRAW_CARET | ctx.DRAWWINDOW_DRAW_VIEW;
    var testRect = gBrowser.getBoundingClientRect();
    if (gIgnoreWindowSize ||
        (0 <= testRect.left &&
         0 <= testRect.top &&
         gContainingWindow.innerWidth >= testRect.right &&
         gContainingWindow.innerHeight >= testRect.bottom)) {
        // We can use the window's retained layer manager
        // because the window is big enough to display the entire
        // browser element
        flags |= ctx.DRAWWINDOW_USE_WIDGET_LAYERS;
    } else if (gBrowserIsRemote) {
        logger.error(gCurrentURL + " | can't drawWindow remote content");
        ++gTestResults.Exception;
    }

    if (gDrawWindowFlags != flags) {
        // Every time the flags change, dump the new state.
        gDrawWindowFlags = flags;
        var flagsStr = "DRAWWINDOW_DRAW_CARET | DRAWWINDOW_DRAW_VIEW";
        if (flags & ctx.DRAWWINDOW_USE_WIDGET_LAYERS) {
            flagsStr += " | DRAWWINDOW_USE_WIDGET_LAYERS";
        } else {
            // Output a special warning because we need to be able to detect
            // this whenever it happens.
            LogWidgetLayersFailure();
            gFailedUseWidgetLayers = true;
        }
        logger.info("drawWindow flags = " + flagsStr +
                    "; window size = " + gContainingWindow.innerWidth + "," + gContainingWindow.innerHeight +
                    "; test browser size = " + testRect.width + "," + testRect.height);
    }

    TestBuffer("DoDrawWindow " + x + "," + y + "," + w + "," + h);
    ctx.drawWindow(gContainingWindow, x, y, w, h, "rgb(255,255,255)",
                   gDrawWindowFlags);
}

function InitCurrentCanvasWithSnapshot()
{
    TestBuffer("Initializing canvas snapshot");

    if (gURLs[0].type == TYPE_LOAD || gURLs[0].type == TYPE_SCRIPT || gURLs[0].type == TYPE_PRINT) {
        // We don't want to snapshot this kind of test
        return false;
    }

    if (!gCurrentCanvas) {
        gCurrentCanvas = AllocateCanvas();
    }

    var ctx = gCurrentCanvas.getContext("2d");
    DoDrawWindow(ctx, 0, 0, gCurrentCanvas.width, gCurrentCanvas.height);
    return true;
}

function UpdateCurrentCanvasForInvalidation(rects)
{
    TestBuffer("Updating canvas for invalidation");

    if (!gCurrentCanvas) {
        return;
    }

    var ctx = gCurrentCanvas.getContext("2d");
    for (var i = 0; i < rects.length; ++i) {
        var r = rects[i];
        // Set left/top/right/bottom to pixel boundaries
        var left = Math.floor(r.left);
        var top = Math.floor(r.top);
        var right = Math.ceil(r.right);
        var bottom = Math.ceil(r.bottom);

        // Clamp the values to the canvas size
        left = Math.max(0, Math.min(left, gCurrentCanvas.width));
        top = Math.max(0, Math.min(top, gCurrentCanvas.height));
        right = Math.max(0, Math.min(right, gCurrentCanvas.width));
        bottom = Math.max(0, Math.min(bottom, gCurrentCanvas.height));

        ctx.save();
        ctx.translate(left, top);
        DoDrawWindow(ctx, left, top, right - left, bottom - top);
        ctx.restore();
    }
}

function UpdateWholeCurrentCanvasForInvalidation()
{
    TestBuffer("Updating entire canvas for invalidation");

    if (!gCurrentCanvas) {
        return;
    }

    var ctx = gCurrentCanvas.getContext("2d");
    DoDrawWindow(ctx, 0, 0, gCurrentCanvas.width, gCurrentCanvas.height);
}

function RecordResult(testRunTime, errorMsg, typeSpecificResults)
{
    TestBuffer("RecordResult fired");

    // Keep track of which test was slowest, and how long it took.
    if (testRunTime > gSlowestTestTime) {
        gSlowestTestTime = testRunTime;
        gSlowestTestURL  = gCurrentURL;
    }

    // Not 'const ...' because of 'EXPECTED_*' value dependency.
    var outputs = {};
    outputs[EXPECTED_PASS] = {
        true:  {s: ["PASS", "PASS"], n: "Pass"},
        false: {s: ["FAIL", "PASS"], n: "UnexpectedFail"}
    };
    outputs[EXPECTED_FAIL] = {
        true:  {s: ["PASS", "FAIL"], n: "UnexpectedPass"},
        false: {s: ["FAIL", "FAIL"], n: "KnownFail"}
    };
    outputs[EXPECTED_RANDOM] = {
        true:  {s: ["PASS", "PASS"], n: "Random"},
        false: {s: ["FAIL", "FAIL"], n: "Random"}
    };
    // for EXPECTED_FUZZY we need special handling because we can have
    // Pass, UnexpectedPass, or UnexpectedFail

    var output;
    var extra;

    if (gURLs[0].type == TYPE_LOAD) {
        ++gTestResults.LoadOnly;
        logger.testEnd(gURLs[0].identifier, "PASS", "PASS", "(LOAD ONLY)");
        gCurrentCanvas = null;
        FinishTestItem();
        return;
    }
    if (gURLs[0].type == TYPE_PRINT) {
        switch (gState) {
        case 1:
            // First document has been loaded.
            gTestPrintOutput = typeSpecificResults;
            // Proceed to load the second document.
            CleanUpCrashDumpFiles();
            StartCurrentURI(2);
            break;
        case 2:
            var pathToTestPdf = gTestPrintOutput;
            var pathToRefPdf = typeSpecificResults;
            comparePdfs(pathToTestPdf, pathToRefPdf, function(error, results) {
                var expected = gURLs[0].expected;
                // TODO: We should complain here if results is empty!
                // (If it's empty, we'll spuriously succeed, regardless of
                // our expectations)
                if (error) {
                    output = outputs[expected][false];
                    extra = { status_msg: output.n };
                    ++gTestResults[output.n];
                    logger.testEnd(gURLs[0].identifier, output.s[0], output.s[1],
                                   error.message, null, extra);
                } else {
                    var outputPair = outputs[expected];
                    if (expected === EXPECTED_FAIL) {
                       var failureResults = results.filter(function (result) { return !result.passed });
                       if (failureResults.length > 0) {
                         // We got an expected failure. Let's get rid of the
                         // passes from the results so we don't trigger
                         // TEST_UNEXPECTED_PASS logging for those.
                         results = failureResults;
                       }
                       // (else, we expected a failure but got none!
                       // Leave results untouched so we can log them.)
                    }
                    results.forEach(function(result) {
                        output = outputPair[result.passed];
                        var extra = { status_msg: output.n };
                        ++gTestResults[output.n];
                        logger.testEnd(gURLs[0].identifier, output.s[0], output.s[1],
                                       result.description, null, extra);
                    });
                }
                FinishTestItem();
            });
            break;
        default:
            throw "Unexpected state.";
        }
        return;
    }
    if (gURLs[0].type == TYPE_SCRIPT) {
        var expected = gURLs[0].expected;

        if (errorMsg) {
            // Force an unexpected failure to alert the test author to fix the test.
            expected = EXPECTED_PASS;
        } else if (typeSpecificResults.length == 0) {
             // This failure may be due to a JavaScript Engine bug causing
             // early termination of the test. If we do not allow silent
             // failure, report an error.
             if (!gURLs[0].allowSilentFail)
                 errorMsg = "No test results reported. (SCRIPT)\n";
             else
                 logger.info("An expected silent failure occurred");
        }

        if (errorMsg) {
            output = outputs[expected][false];
            extra = { status_msg: output.n };
            ++gTestResults[output.n];
            logger.testEnd(gURLs[0].identifier, output.s[0], output.s[1], errorMsg, null, extra);
            FinishTestItem();
            return;
        }

        var anyFailed = typeSpecificResults.some(function(result) { return !result.passed; });
        var outputPair;
        if (anyFailed && expected == EXPECTED_FAIL) {
            // If we're marked as expected to fail, and some (but not all) tests
            // passed, treat those tests as though they were marked random
            // (since we can't tell whether they were really intended to be
            // marked failing or not).
            outputPair = { true: outputs[EXPECTED_RANDOM][true],
                           false: outputs[expected][false] };
        } else {
            outputPair = outputs[expected];
        }
        var index = 0;
        typeSpecificResults.forEach(function(result) {
                var output = outputPair[result.passed];
                var extra = { status_msg: output.n };

                ++gTestResults[output.n];
                logger.testEnd(gURLs[0].identifier, output.s[0], output.s[1],
                               result.description + " item " + (++index), null, extra);
            });

        if (anyFailed && expected == EXPECTED_PASS) {
            FlushTestBuffer();
        }

        FinishTestItem();
        return;
    }

    if (gURLs[0]["prefSettings" + gState].length == 0 &&
        gURICanvases[gCurrentURL]) {
        gCurrentCanvas = gURICanvases[gCurrentURL];
    }
    if (gCurrentCanvas == null) {
        logger.error(gCurrentURL, "program error managing snapshots");
        ++gTestResults.Exception;
    }
    if (gState == 1) {
        gCanvas1 = gCurrentCanvas;
    } else {
        gCanvas2 = gCurrentCanvas;
    }
    gCurrentCanvas = null;

    ResetRenderingState();

    switch (gState) {
        case 1:
            // First document has been loaded.
            // Proceed to load the second document.

            CleanUpCrashDumpFiles();
            StartCurrentURI(2);
            break;
        case 2:
            // Both documents have been loaded. Compare the renderings and see
            // if the comparison result matches the expected result specified
            // in the manifest.

            // number of different pixels
            var differences;
            // whether the two renderings match:
            var equal;
            var maxDifference = {};
            // whether the allowed fuzziness from the annotations is exceeded
            // by the actual comparison results
            var fuzz_exceeded = false;

            differences = gWindowUtils.compareCanvases(gCanvas1, gCanvas2, maxDifference);
            equal = (differences == 0);

            if (maxDifference.value > 0 && equal) {
                throw "Inconsistent result from compareCanvases.";
            }

            // what is expected on this platform (PASS, FAIL, or RANDOM)
            var expected = gURLs[0].expected;

            if (expected == EXPECTED_FUZZY) {
                logger.info(`REFTEST fuzzy test ` +
                            `(${gURLs[0].fuzzyMinDelta}, ${gURLs[0].fuzzyMinPixels}) <= ` +
                            `(${maxDifference.value}, ${differences}) <= ` +
                            `(${gURLs[0].fuzzyMaxDelta}, ${gURLs[0].fuzzyMaxPixels})`);
                fuzz_exceeded = maxDifference.value > gURLs[0].fuzzyMaxDelta ||
                                differences > gURLs[0].fuzzyMaxPixels;
                equal = !fuzz_exceeded &&
                        maxDifference.value >= gURLs[0].fuzzyMinDelta &&
                        differences >= gURLs[0].fuzzyMinPixels;
            }

            var failedExtraCheck = gFailedNoPaint || gFailedOpaqueLayer || gFailedAssignedLayer;

            // whether the comparison result matches what is in the manifest
            var test_passed = (equal == (gURLs[0].type == TYPE_REFTEST_EQUAL)) && !failedExtraCheck;

            if (expected != EXPECTED_FUZZY) {
                output = outputs[expected][test_passed];
            } else if (test_passed) {
                output = {s: ["PASS", "PASS"], n: "Pass"};
            } else if (gURLs[0].type == TYPE_REFTEST_EQUAL &&
                       !failedExtraCheck &&
                       !fuzz_exceeded) {
                // If we get here, that means we had an '==' type test where
                // at least one of the actual difference values was below the
                // allowed range, but nothing else was wrong. So let's produce
                // UNEXPECTED-PASS in this scenario. Also, if we enter this
                // branch, 'equal' must be false so let's assert that to guard
                // against logic errors.
                if (equal) {
                    throw "Logic error in reftest.jsm fuzzy test handling!";
                }
                output = {s: ["PASS", "FAIL"], n: "UnexpectedPass"};
            } else {
                // In all other cases we fail the test
                output = {s: ["FAIL", "PASS"], n: "UnexpectedFail"};
            }
            extra = { status_msg: output.n };

            ++gTestResults[output.n];

            // It's possible that we failed both an "extra check" and the normal comparison, but we don't
            // have a way to annotate these separately, so just print an error for the extra check failures.
            if (failedExtraCheck) {
                var failures = [];
                if (gFailedNoPaint) {
                    failures.push("failed reftest-no-paint");
                }
                // The gFailed*Messages arrays will contain messages from both the test and the reference.
                if (gFailedOpaqueLayer) {
                    failures.push("failed reftest-opaque-layer: " + gFailedOpaqueLayerMessages.join(", "));
                }
                if (gFailedAssignedLayer) {
                    failures.push("failed reftest-assigned-layer: " + gFailedAssignedLayerMessages.join(", "));
                }
                var failureString = failures.join(", ");
                logger.testEnd(gURLs[0].identifier, output.s[0], output.s[1], failureString, null, extra);
            } else {
                var message = "image comparison";
                if (!test_passed && expected == EXPECTED_PASS ||
                    !test_passed && expected == EXPECTED_FUZZY ||
                    test_passed && expected == EXPECTED_FAIL) {
                    if (!equal) {
                        extra.max_difference = maxDifference.value;
                        extra.differences = differences;
                        var image1 = gCanvas1.toDataURL();
                        var image2 = gCanvas2.toDataURL();
                        extra.reftest_screenshots = [
                            {url:gURLs[0].identifier[0],
                             screenshot: image1.slice(image1.indexOf(",") + 1)},
                            gURLs[0].identifier[1],
                            {url:gURLs[0].identifier[2],
                             screenshot: image2.slice(image2.indexOf(",") + 1)}
                        ];
                        extra.image1 = image1;
                        extra.image2 = image2;
                        message += (", max difference: " + extra.max_difference +
                                    ", number of differing pixels: " + differences);
                    } else {
                        var image1 = gCanvas1.toDataURL();
                        extra.reftest_screenshots = [
                            {url:gURLs[0].identifier[0],
                             screenshot: image1.slice(image1.indexOf(",") + 1)}
                        ];
                        extra.image1 = image1;
                    }
                }
                logger.testEnd(gURLs[0].identifier, output.s[0], output.s[1], message, null, extra);

                if (gNoCanvasCache) {
                    ReleaseCanvas(gCanvas1);
                    ReleaseCanvas(gCanvas2);
                } else {
                    if (gURLs[0].prefSettings1.length == 0) {
                        UpdateCanvasCache(gURLs[0].url1, gCanvas1);
                    }
                    if (gURLs[0].prefSettings2.length == 0) {
                        UpdateCanvasCache(gURLs[0].url2, gCanvas2);
                    }
                }
            }

            if ((!test_passed && expected == EXPECTED_PASS) || (test_passed && expected == EXPECTED_FAIL)) {
                FlushTestBuffer();
            }

            CleanUpCrashDumpFiles();
            FinishTestItem();
            break;
        default:
            throw "Unexpected state.";
    }
}

function LoadFailed(why)
{
    ++gTestResults.FailedLoad;
    // Once bug 896840 is fixed, this can go away, but for now it will give log
    // output that is TBPL starable for bug 789751 and bug 720452.
    if (!why) {
        logger.error("load failed with unknown reason");
    }
    logger.testEnd(gURLs[0]["url" + gState].spec, "FAIL", "PASS", "load failed: " + why);
    FlushTestBuffer();
    FinishTestItem();
}

function RemoveExpectedCrashDumpFiles()
{
    if (gExpectingProcessCrash) {
        for (let crashFilename of gExpectedCrashDumpFiles) {
            let file = gCrashDumpDir.clone();
            file.append(crashFilename);
            if (file.exists()) {
                file.remove(false);
            }
        }
    }
    gExpectedCrashDumpFiles.length = 0;
}

function FindUnexpectedCrashDumpFiles()
{
    if (!gCrashDumpDir.exists()) {
        return;
    }

    let entries = gCrashDumpDir.directoryEntries;
    if (!entries) {
        return;
    }

    let foundCrashDumpFile = false;
    while (entries.hasMoreElements()) {
        let file = entries.getNext().QueryInterface(CI.nsIFile);
        let path = String(file.path);
        if (path.match(/\.(dmp|extra)$/) && !gUnexpectedCrashDumpFiles[path]) {
            if (!foundCrashDumpFile) {
                ++gTestResults.UnexpectedFail;
                foundCrashDumpFile = true;
                if (gCurrentURL) {
                    logger.testEnd(gCurrentURL, "FAIL", "PASS", "This test left crash dumps behind, but we weren't expecting it to!");
                } else {
                    logger.error("Harness startup left crash dumps behind, but we weren't expecting it to!");
                }
            }
            logger.info("Found unexpected crash dump file " + path);
            gUnexpectedCrashDumpFiles[path] = true;
        }
    }
}

function RemovePendingCrashDumpFiles()
{
    if (!gPendingCrashDumpDir.exists()) {
        return;
    }

    let entries = gPendingCrashDumpDir.directoryEntries;
    while (entries.hasMoreElements()) {
        let file = entries.getNext().QueryInterface(CI.nsIFile);
        if (file.isFile()) {
          file.remove(false);
          logger.info("This test left pending crash dumps; deleted "+file.path);
        }
    }
}

function CleanUpCrashDumpFiles()
{
    RemoveExpectedCrashDumpFiles();
    FindUnexpectedCrashDumpFiles();
    if (gCleanupPendingCrashes) {
      RemovePendingCrashDumpFiles();
    }
    gExpectingProcessCrash = false;
}

function FinishTestItem()
{
    // Replace document with BLANK_URL_FOR_CLEARING in case there are
    // assertions when unloading.
    logger.debug("Loading a blank page");
    // After clearing, content will notify us of the assertion count
    // and tests will continue.
    SendClear();
    gFailedNoPaint = false;
    gFailedOpaqueLayer = false;
    gFailedOpaqueLayerMessages = [];
    gFailedAssignedLayer = false;
    gFailedAssignedLayerMessages = [];
}

function DoAssertionCheck(numAsserts)
{
    if (gDebug.isDebugBuild) {
        if (gBrowserIsRemote) {
            // Count chrome-process asserts too when content is out of
            // process.
            var newAssertionCount = gDebug.assertionCount;
            var numLocalAsserts = newAssertionCount - gAssertionCount;
            gAssertionCount = newAssertionCount;

            numAsserts += numLocalAsserts;
        }

        var minAsserts = gURLs[0].minAsserts;
        var maxAsserts = gURLs[0].maxAsserts;

        logger.assertionCount(gCurrentURL, numAsserts, minAsserts, maxAsserts);
    }

    if (gURLs[0].chaosMode) {
        gWindowUtils.leaveChaosMode();
    }

    // And start the next test.
    gURLs.shift();
    StartCurrentTest();
}

function ResetRenderingState()
{
    SendResetRenderingState();
    // We would want to clear any viewconfig here, if we add support for it
}

function RestoreChangedPreferences()
{
    if (gPrefsToRestore.length > 0) {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                    getService(Components.interfaces.nsIPrefBranch);
        gPrefsToRestore.reverse();
        gPrefsToRestore.forEach(function(ps) {
            var value = ps.value;
            if (ps.type == PREF_BOOLEAN) {
                prefs.setBoolPref(ps.name, value);
            } else if (ps.type == PREF_STRING) {
                prefs.setCharPref(ps.name, value);
                value = '"' + value + '"';
            } else if (ps.type == PREF_INTEGER) {
                prefs.setIntPref(ps.name, value);
            }
            logger.info("RESTORE PREFERENCE pref(" + ps.name + "," + value + ")");
        });
        gPrefsToRestore = [];
    }
}

function RegisterMessageListenersAndLoadContentScript()
{
    gBrowserMessageManager.addMessageListener(
        "reftest:AssertionCount",
        function (m) { RecvAssertionCount(m.json.count); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:ContentReady",
        function (m) { return RecvContentReady(m.data); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:Exception",
        function (m) { RecvException(m.json.what) }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:FailedLoad",
        function (m) { RecvFailedLoad(m.json.why); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:FailedNoPaint",
        function (m) { RecvFailedNoPaint(); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:FailedOpaqueLayer",
        function (m) { RecvFailedOpaqueLayer(m.json.why); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:FailedAssignedLayer",
        function (m) { RecvFailedAssignedLayer(m.json.why); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:InitCanvasWithSnapshot",
        function (m) { return RecvInitCanvasWithSnapshot(); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:Log",
        function (m) { RecvLog(m.json.type, m.json.msg); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:ScriptResults",
        function (m) { RecvScriptResults(m.json.runtimeMs, m.json.error, m.json.results); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:PrintResult",
        function (m) { RecvPrintResult(m.json.runtimeMs, m.json.status, m.json.fileName); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:TestDone",
        function (m) { RecvTestDone(m.json.runtimeMs); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:UpdateCanvasForInvalidation",
        function (m) { RecvUpdateCanvasForInvalidation(m.json.rects); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:UpdateWholeCanvasForInvalidation",
        function (m) { RecvUpdateWholeCanvasForInvalidation(); }
    );
    gBrowserMessageManager.addMessageListener(
        "reftest:ExpectProcessCrash",
        function (m) { RecvExpectProcessCrash(); }
    );

    gBrowserMessageManager.loadFrameScript("chrome://reftest/content/reftest-content.js", true, true);
}

function RecvAssertionCount(count)
{
    DoAssertionCheck(count);
}

function RecvContentReady(info)
{
    gContentGfxInfo = info.gfx;
    InitAndStartRefTests();
    return { remote: gBrowserIsRemote };
}

function RecvException(what)
{
    logger.error(gCurrentURL + " | " + what);
    ++gTestResults.Exception;
}

function RecvFailedLoad(why)
{
    LoadFailed(why);
}

function RecvFailedNoPaint()
{
    gFailedNoPaint = true;
}

function RecvFailedOpaqueLayer(why) {
    gFailedOpaqueLayer = true;
    gFailedOpaqueLayerMessages.push(why);
}

function RecvFailedAssignedLayer(why) {
    gFailedAssignedLayer = true;
    gFailedAssignedLayerMessages.push(why);
}

function RecvInitCanvasWithSnapshot()
{
    var painted = InitCurrentCanvasWithSnapshot();
    return { painted: painted };
}

function RecvLog(type, msg)
{
    msg = "[CONTENT] " + msg;
    if (type == "info") {
        TestBuffer(msg);
    } else if (type == "warning") {
        logger.warning(msg);
    } else {
        logger.error("REFTEST TEST-UNEXPECTED-FAIL | " + gCurrentURL + " | unknown log type " + type + "\n");
        ++gTestResults.Exception;
    }
}

function RecvScriptResults(runtimeMs, error, results)
{
    RecordResult(runtimeMs, error, results);
}

function RecvPrintResult(runtimeMs, status, fileName)
{
    if (!Components.isSuccessCode(status)) {
      logger.error("REFTEST TEST-UNEXPECTED-FAIL | " + gCurrentURL + " | error during printing\n");
      ++gTestResults.Exception;
    }
    RecordResult(runtimeMs, '', fileName);
}

function RecvTestDone(runtimeMs)
{
    RecordResult(runtimeMs, '', [ ]);
}

function RecvUpdateCanvasForInvalidation(rects)
{
    UpdateCurrentCanvasForInvalidation(rects);
}

function RecvUpdateWholeCanvasForInvalidation()
{
    UpdateWholeCurrentCanvasForInvalidation();
}

function OnProcessCrashed(subject, topic, data)
{
    var id;
    subject = subject.QueryInterface(CI.nsIPropertyBag2);
    if (topic == "plugin-crashed") {
        id = subject.getPropertyAsAString("pluginDumpID");
    } else if (topic == "ipc:content-shutdown") {
        id = subject.getPropertyAsAString("dumpID");
    }
    if (id) {
        gExpectedCrashDumpFiles.push(id + ".dmp");
        gExpectedCrashDumpFiles.push(id + ".extra");
    }
}

function RegisterProcessCrashObservers()
{
    var os = CC[NS_OBSERVER_SERVICE_CONTRACTID]
             .getService(CI.nsIObserverService);
    os.addObserver(OnProcessCrashed, "plugin-crashed");
    os.addObserver(OnProcessCrashed, "ipc:content-shutdown");
}

function RecvExpectProcessCrash()
{
    gExpectingProcessCrash = true;
}

function SendClear()
{
    gBrowserMessageManager.sendAsyncMessage("reftest:Clear");
}

function SendLoadScriptTest(uri, timeout)
{
    gBrowserMessageManager.sendAsyncMessage("reftest:LoadScriptTest",
                                            { uri: uri, timeout: timeout });
}

function SendLoadPrintTest(uri, timeout)
{
    gBrowserMessageManager.sendAsyncMessage("reftest:LoadPrintTest",
                                            { uri: uri, timeout: timeout });
}

function SendLoadTest(type, uri, timeout)
{
    gBrowserMessageManager.sendAsyncMessage("reftest:LoadTest",
                                            { type: type, uri: uri, timeout: timeout }
    );
}

function SendResetRenderingState()
{
    gBrowserMessageManager.sendAsyncMessage("reftest:ResetRenderingState");
}

function readPdf(path, callback) {
    OS.File.open(path, { read: true }).then(function (file) {
        file.flush().then(function() {
            file.read().then(function (data) {
                var fakePort = new PDFJS.main.LoopbackPort(true);
                PDFJS.worker.WorkerMessageHandler.initializeFromPort(fakePort);
                var myWorker = new PDFJS.main.PDFWorker("worker", fakePort);
                PDFJS.main.PDFJS.getDocument({
                    worker: myWorker,
                    data: data
                }).then(function (pdf) {
                    callback(null, pdf);
                }, function () {
                    callback(new Error("Couldn't parse " + path));
                });
                return;
            }, function () {
                callback(new Error("Couldn't read PDF"));
            });
        });
    });
}

function comparePdfs(pathToTestPdf, pathToRefPdf, callback) {
    Promise.all([pathToTestPdf, pathToRefPdf].map(function(path) {
        return new Promise(function(resolve, reject) {
            readPdf(path, function(error, pdf) {
                // Resolve or reject outer promise. reject and resolve are
                // passed to the callback function given as first arguments
                // to the Promise constructor.
                if (error) {
                    reject(error);
                } else {
                    resolve(pdf);
                }
            });
        });
    })).then(function(pdfs) {
        var numberOfPages = pdfs[1].numPages;
        var sameNumberOfPages = numberOfPages === pdfs[0].numPages;

        var resultPromises = [Promise.resolve({
            passed: sameNumberOfPages,
            description: "Expected number of pages: " + numberOfPages +
                                             ", got " + pdfs[0].numPages
        })];

        if (sameNumberOfPages) {
            for (var i = 0; i < numberOfPages; i++) {
                var pageNum = i + 1;
                var testPagePromise = pdfs[0].getPage(pageNum);
                var refPagePromise = pdfs[1].getPage(pageNum);
                resultPromises.push(new Promise(function(resolve, reject) {
                    Promise.all([testPagePromise, refPagePromise]).then(function(pages) {
                        var testTextPromise = pages[0].getTextContent();
                        var refTextPromise = pages[1].getTextContent();
                        Promise.all([testTextPromise, refTextPromise]).then(function(texts) {
                            var testTextItems = texts[0].items;
                            var refTextItems = texts[1].items;
                            var testText;
                            var refText;
                            var passed = refTextItems.every(function(o, i) {
                                refText = o.str;
                                if (!testTextItems[i]) {
                                    return false;
                                }
                                testText = testTextItems[i].str;
                                return testText === refText;
                            });
                            var description;
                            if (passed) {
                                if (testTextItems.length > refTextItems.length) {
                                    passed = false;
                                    description = "Page " + pages[0].pageNumber +
                                        " contains unexpected text like '" +
                                        testTextItems[refTextItems.length].str + "'";
                                } else {
                                    description = "Page " + pages[0].pageNumber +
                                        " contains same text"
                                }
                            } else {
                                description = "Expected page " + pages[0].pageNumber +
                                    " to contain text '" + refText;
                                if (testText) {
                                    description += "' but found '" + testText +
                                        "' instead";
                                }
                            }
                            resolve({
                                passed: passed,
                                description: description
                            });
                        }, reject);
                    }, reject);
                }));
            }
        }

        Promise.all(resultPromises).then(function (results) {
            callback(null, results);
        });
    }, function(error) {
        callback(error);
    });
}
