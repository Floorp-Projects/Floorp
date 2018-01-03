/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = [
    "OnRefTestLoad",
    "OnRefTestUnload",
    "getTestPlugin"
];

var CC = Components.classes;
const CI = Components.interfaces;
const CR = Components.results;
const CU = Components.utils;

CU.import("resource://gre/modules/FileUtils.jsm");
CU.import("chrome://reftest/content/globals.jsm", this);
CU.import("chrome://reftest/content/httpd.jsm", this);
CU.import("chrome://reftest/content/manifest.jsm", this);
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

function HasUnexpectedResult()
{
    return g.testResults.Exception > 0 ||
           g.testResults.FailedLoad > 0 ||
           g.testResults.UnexpectedFail > 0 ||
           g.testResults.UnexpectedPass > 0 ||
           g.testResults.AssertionUnexpected > 0 ||
           g.testResults.AssertionUnexpectedFixed > 0;
}

// By default we just log to stdout
var gDumpFn = function(line) {
  dump(line);
  if (g.logFile) {
    g.logFile.write(line, line.length);
  }
}
var gDumpRawLog = function(record) {
  // Dump JSON representation of data on a single line
  var line = "\n" + JSON.stringify(record) + "\n";
  dump(line);

  if (g.logFile) {
    g.logFile.write(line, line.length);
  }
}
g.logger = new StructuredLogger('reftest', gDumpRawLog);
var logger = g.logger;

function TestBuffer(str)
{
  logger.debug(str);
  g.testLog.push(str);
}

function FlushTestBuffer()
{
  // In debug mode, we've dumped all these messages already.
  if (g.logLevel !== 'debug') {
    for (var i = 0; i < g.testLog.length; ++i) {
      logger.info("Saved log: " + g.testLog[i]);
    }
  }
  g.testLog = [];
}

function LogWidgetLayersFailure()
{
  logger.error(
    "Screen resolution is too low - USE_WIDGET_LAYERS was disabled. " +
    (g.browserIsRemote ?
      "Since E10s is enabled, there is no fallback rendering path!" :
      "The fallback rendering path is not reliably consistent with on-screen rendering."));

  logger.error(
    "If you cannot increase your screen resolution you can try reducing " +
    "gecko's pixel scaling by adding something like '--setpref " +
    "layout.css.devPixelsPerPx=1.0' to your './mach reftest' command " +
    "(possibly as an alias in ~/.mozbuild/machrc). Note that this is " +
    "inconsistent with CI testing, and may interfere with HighDPI/" +
    "reftest-zoom tests.");
}

function AllocateCanvas()
{
    if (g.recycledCanvases.length > 0) {
        return g.recycledCanvases.shift();
    }

    var canvas = g.containingWindow.document.createElementNS(XHTML_NS, "canvas");
    var r = g.browser.getBoundingClientRect();
    canvas.setAttribute("width", Math.ceil(r.width));
    canvas.setAttribute("height", Math.ceil(r.height));

    return canvas;
}

function ReleaseCanvas(canvas)
{
    // store a maximum of 2 canvases, if we're not caching
    if (!g.noCanvasCache || g.recycledCanvases.length < 2) {
        g.recycledCanvases.push(canvas);
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

function OnRefTestLoad(win)
{
    g.crashDumpDir = CC[NS_DIRECTORY_SERVICE_CONTRACTID]
                    .getService(CI.nsIProperties)
                    .get("ProfD", CI.nsIFile);
    g.crashDumpDir.append("minidumps");

    g.pendingCrashDumpDir = CC[NS_DIRECTORY_SERVICE_CONTRACTID]
                    .getService(CI.nsIProperties)
                    .get("UAppData", CI.nsIFile);
    g.pendingCrashDumpDir.append("Crash Reports");
    g.pendingCrashDumpDir.append("pending");

    var env = CC["@mozilla.org/process/environment;1"].
              getService(CI.nsIEnvironment);

    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);
    g.browserIsRemote = prefs.getBoolPref("browser.tabs.remote.autostart", false);

    g.browserIsIframe = prefs.getBoolPref("reftest.browser.iframe.enabled", false);

    g.logLevel = prefs.getCharPref("reftest.logLevel", "info");

    if (win === undefined || win == null) {
      win = window;
    }
    if (g.containingWindow == null && win != null) {
      g.containingWindow = win;
    }

    if (g.browserIsIframe) {
      g.browser = g.containingWindow.document.createElementNS(XHTML_NS, "iframe");
      g.browser.setAttribute("mozbrowser", "");
    } else {
      g.browser = g.containingWindow.document.createElementNS(XUL_NS, "xul:browser");
      g.browser.setAttribute("class", "lightweight");
    }
    g.browser.setAttribute("id", "browser");
    g.browser.setAttribute("type", "content");
    g.browser.setAttribute("primary", "true");
    g.browser.setAttribute("remote", g.browserIsRemote ? "true" : "false");
    // Make sure the browser element is exactly 800x1000, no matter
    // what size our window is
    g.browser.setAttribute("style", "padding: 0px; margin: 0px; border:none; min-width: 800px; min-height: 1000px; max-width: 800px; max-height: 1000px");

    if (Services.appinfo.OS == "Android") {
      let doc = g.containingWindow.document.getElementById('main-window');
      while (doc.hasChildNodes()) {
        doc.firstChild.remove();
      }
      doc.appendChild(g.browser);
    } else {
      document.getElementById("reftest-window").appendChild(g.browser);
    }

    // reftests should have the test plugins enabled, not click-to-play
    let plugin1 = getTestPlugin("Test Plug-in");
    let plugin2 = getTestPlugin("Second Test Plug-in");
    if (plugin1 && plugin2) {
      g.testPluginEnabledStates = [plugin1.enabledState, plugin2.enabledState];
      plugin1.enabledState = CI.nsIPluginTag.STATE_ENABLED;
      plugin2.enabledState = CI.nsIPluginTag.STATE_ENABLED;
    } else {
      logger.warning("Could not get test plugin tags.");
    }

    g.browserMessageManager = g.browser.frameLoader.messageManager;
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

    /* set the g.loadTimeout */
    try {
        g.loadTimeout = prefs.getIntPref("reftest.timeout");
    } catch(e) {
        g.loadTimeout = 5 * 60 * 1000; //5 minutes as per bug 479518
    }

    /* Get the logfile for android tests */
    try {
        var logFile = prefs.getCharPref("reftest.logFile");
        if (logFile) {
            var f = FileUtils.File(logFile);
            g.logFile = FileUtils.openFileOutputStream(f, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE);
        }
    } catch(e) {}

    g.remote = prefs.getBoolPref("reftest.remote", false);

    g.ignoreWindowSize = prefs.getBoolPref("reftest.ignoreWindowSize", false);

    /* Support for running a chunk (subset) of tests.  In separate try as this is optional */
    try {
        g.totalChunks = prefs.getIntPref("reftest.totalChunks");
        g.thisChunk = prefs.getIntPref("reftest.thisChunk");
    }
    catch(e) {
        g.totalChunks = 0;
        g.thisChunk = 0;
    }

    try {
        g.focusFilterMode = prefs.getCharPref("reftest.focusFilterMode");
    } catch(e) {}

    try {
        g.startAfter = prefs.getCharPref("reftest.startAfter");
    } catch(e) {
        g.startAfter = undefined;
    }

    try {
        g.compareRetainedDisplayLists = prefs.getBoolPref("reftest.compareRetainedDisplayLists");
    } catch (e) {}
#ifdef MOZ_STYLO
    try {
        g.compareStyloToGecko = prefs.getBoolPref("reftest.compareStyloToGecko");
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

    g.windowUtils = g.containingWindow.QueryInterface(CI.nsIInterfaceRequestor).getInterface(CI.nsIDOMWindowUtils);
    if (!g.windowUtils || !g.windowUtils.compareCanvases)
        throw "nsIDOMWindowUtils inteface missing";

    g.ioService = CC[IO_SERVICE_CONTRACTID].getService(CI.nsIIOService);
    g.debug = CC[DEBUG_CONTRACTID].getService(CI.nsIDebug2);

    RegisterProcessCrashObservers();

    if (g.remote) {
        g.server = null;
    } else {
        g.server = new HttpServer();
    }
    try {
        if (g.server)
            StartHTTPServer();
    } catch (ex) {
        //g.browser.loadURI('data:text/plain,' + ex);
        ++g.testResults.Exception;
        logger.error("EXCEPTION: " + ex);
        DoneTests();
    }

    // Focus the content browser.
    if (g.focusFilterMode != FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS) {
        g.browser.addEventListener("focus", StartTests, true);
        g.browser.focus();
    } else {
        StartTests();
    }
}

function StartHTTPServer()
{
    g.server.registerContentType("sjs", "sjs");
    g.server.start(-1);
    g.httpServerPort = g.server.identity.primaryPort;
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
    if (g.focusFilterMode != FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS) {
        g.browser.removeEventListener("focus", StartTests, true);
    }

    var manifests;
    /* These prefs are optional, so we don't need to spit an error to the log */
    try {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                    getService(Components.interfaces.nsIPrefBranch);
    } catch(e) {
        logger.error("EXCEPTION: " + e);
    }

    g.noCanvasCache = prefs.getIntPref("reftest.nocache", false);

    g.shuffle = prefs.getBoolPref("reftest.shuffle", false);

    g.runUntilFailure = prefs.getBoolPref("reftest.runUntilFailure", false);

    g.verify = prefs.getBoolPref("reftest.verify", false);

    g.cleanupPendingCrashes = prefs.getBoolPref("reftest.cleanupPendingCrashes", false);

    // Check if there are any crash dump files from the startup procedure, before
    // we start running the first test. Otherwise the first test might get
    // blamed for producing a crash dump file when that was not the case.
    CleanUpCrashDumpFiles();

    // When we repeat this function is called again, so really only want to set
    // g.repeat once.
    if (g.repeat == null) {
      g.repeat = prefs.getIntPref("reftest.repeat", 0);
    }

    g.runSlowTests = prefs.getIntPref("reftest.skipslowtests", false);

    if (g.shuffle) {
        g.noCanvasCache = true;
    }

    g.urls = [];

    try {
        var manifests = JSON.parse(prefs.getCharPref("reftest.manifests"));
        g.urlsFilterRegex = manifests[null];
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
        for (var i = 0; i < g.urls.length; ++i) {
            if (g.urls[i].expected == EXPECTED_DEATH)
                continue;

            if (g.urls[i].needsFocus && !Focus())
                continue;

            if (g.urls[i].slow && !g.runSlowTests)
                continue;

            tURLs.push(g.urls[i]);
        }

        var numActiveTests = tURLs.length;

        if (g.totalChunks > 0 && g.thisChunk > 0) {
            // Calculate start and end indices of this chunk if tURLs array were
            // divided evenly
            var testsPerChunk = tURLs.length / g.totalChunks;
            var start = Math.round((g.thisChunk-1) * testsPerChunk);
            var end = Math.round(g.thisChunk * testsPerChunk);
            numActiveTests = end - start;

            // Map these indices onto the g.urls array. This avoids modifying the
            // g.urls array which prevents skipped tests from showing up in the log
            start = g.thisChunk == 1 ? 0 : g.urls.indexOf(tURLs[start]);
            end = g.thisChunk == g.totalChunks ? g.urls.length : g.urls.indexOf(tURLs[end + 1]) - 1;

            logger.info("Running chunk " + g.thisChunk + " out of " + g.totalChunks + " chunks.  " +
                "tests " + (start+1) + "-" + end + "/" + g.urls.length);

            g.urls = g.urls.slice(start, end);
        }

        if (g.startAfter === undefined && !g.suiteStarted) {
            var ids = g.urls.map(function(obj) {
                return obj.identifier;
            });
            var suite = prefs.getCharPref('reftest.suite', 'reftest');
            logger.suiteStart(ids, suite, {"skipped": g.urls.length - numActiveTests});
            g.suiteStarted = true
        }

        if (g.shuffle) {
            if (g.startAfter !== undefined) {
                logger.error("Can't resume from a crashed test when " +
                             "--shuffle is enabled, continue by shuffling " +
                             "all the tests");
                DoneTests();
                return;
            }
            Shuffle(g.urls);
        } else if (g.startAfter !== undefined) {
            // Skip through previously crashed test
            // We have to do this after chunking so we don't break the numbers
            var crash_idx = g.urls.map(function(url) {
                return url['url1']['spec'];
            }).indexOf(g.startAfter);
            if (crash_idx == -1) {
                throw "Can't find the previously crashed test";
            }
            g.urls = g.urls.slice(crash_idx + 1);
        }

        g.totalTests = g.urls.length;

        if (!g.totalTests && !g.verify)
            throw "No tests to run";

        g.uriCanvases = {};
        StartCurrentTest();
    } catch (ex) {
        //g.browser.loadURI('data:text/plain,' + ex);
        ++g.testResults.Exception;
        logger.error("EXCEPTION: " + ex);
        DoneTests();
    }
}

function OnRefTestUnload()
{
  let plugin1 = getTestPlugin("Test Plug-in");
  let plugin2 = getTestPlugin("Second Test Plug-in");
  if (plugin1 && plugin2) {
    plugin1.enabledState = g.testPluginEnabledStates[0];
    plugin2.enabledState = g.testPluginEnabledStates[1];
  } else {
    logger.warning("Failed to get test plugin tags.");
  }
}

function AddURIUseCount(uri)
{
    if (uri == null)
        return;

    var spec = uri.spec;
    if (spec in g.uriUseCounts) {
        g.uriUseCounts[spec]++;
    } else {
        g.uriUseCounts[spec] = 1;
    }
}

function BuildUseCounts()
{
    if (g.noCanvasCache) {
        return;
    }

    g.uriUseCounts = {};
    for (var i = 0; i < g.urls.length; ++i) {
        var url = g.urls[i];
        if (url.expected != EXPECTED_DEATH &&
            (url.type == TYPE_REFTEST_EQUAL ||
             url.type == TYPE_REFTEST_NOTEQUAL)) {
            if (url.prefSettings1.length == 0) {
                AddURIUseCount(g.urls[i].url1);
            }
            if (url.prefSettings2.length == 0) {
                AddURIUseCount(g.urls[i].url2);
            }
        }
    }
}

// Return true iff this window is focused when this function returns.
function Focus()
{
    var fm = CC["@mozilla.org/focus-manager;1"].getService(CI.nsIFocusManager);
    fm.focusedWindow = g.containingWindow;
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
    g.containingWindow.blur();
}

function StartCurrentTest()
{
    g.testLog = [];

    // make sure we don't run tests that are expected to kill the browser
    while (g.urls.length > 0) {
        var test = g.urls[0];
        logger.testStart(test.identifier);
        if (test.expected == EXPECTED_DEATH) {
            ++g.testResults.Skip;
            logger.testEnd(test.identifier, "SKIP");
            g.urls.shift();
        } else if (test.needsFocus && !Focus()) {
            // FIXME: Marking this as a known fail is dangerous!  What
            // if it starts failing all the time?
            ++g.testResults.Skip;
            logger.testEnd(test.identifier, "SKIP", null, "(COULDN'T GET FOCUS)");
            g.urls.shift();
        } else if (test.slow && !g.runSlowTests) {
            ++g.testResults.Slow;
            logger.testEnd(test.identifier, "SKIP", null, "(SLOW)");
            g.urls.shift();
        } else {
            break;
        }
    }

    if ((g.urls.length == 0 && g.repeat == 0) ||
        (g.runUntilFailure && HasUnexpectedResult())) {
        RestoreChangedPreferences();
        DoneTests();
    } else if (g.urls.length == 0 && g.repeat > 0) {
        // Repeat
        g.repeat--;
        StartTests();
    } else {
        if (g.urls[0].chaosMode) {
            g.windowUtils.enterChaosMode();
        }
        if (!g.urls[0].needsFocus) {
            Blur();
        }
        var currentTest = g.totalTests - g.urls.length;
        g.containingWindow.document.title = "reftest: " + currentTest + " / " + g.totalTests +
            " (" + Math.floor(100 * (currentTest / g.totalTests)) + "%)";
        StartCurrentURI(URL_TARGET_TYPE_TEST);
    }
}

function StartCurrentURI(aURLTargetType)
{
    const isStartingRef = (aURLTargetType == URL_TARGET_TYPE_REFERENCE);

    g.currentURL = g.urls[0][isStartingRef ? "url2" : "url1"].spec;
    g.currentURLTargetType = aURLTargetType;

    RestoreChangedPreferences();

    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
        getService(Components.interfaces.nsIPrefBranch);

    const prefSettings =
      g.urls[0][isStartingRef ? "prefSettings2" : "prefSettings1"];

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
                    g.prefsToRestore.push( { name: ps.name,
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
                var test = g.urls[0];
                if (test.expected == EXPECTED_FAIL) {
                    logger.testEnd(test.identifier, "FAIL", "FAIL",
                                   "(SKIPPED; " + badPref + " not known or wrong type)");
                    ++g.testResults.Skip;
                } else {
                    logger.testEnd(test.identifier, "FAIL", "PASS",
                                   badPref + " not known or wrong type");
                    ++g.testResults.UnexpectedFail;
                }

                // skip the test that had a bad preference
                g.urls.shift();
                StartCurrentTest();
                return;
            } else {
                throw e;
            }
        }
    }

    if (prefSettings.length == 0 &&
        g.uriCanvases[g.currentURL] &&
        (g.urls[0].type == TYPE_REFTEST_EQUAL ||
         g.urls[0].type == TYPE_REFTEST_NOTEQUAL) &&
        g.urls[0].maxAsserts == 0) {
        // Pretend the document loaded --- RecordResult will notice
        // there's already a canvas for this URL
        g.containingWindow.setTimeout(RecordResult, 0);
    } else {
        var currentTest = g.totalTests - g.urls.length;
        // Log this to preserve the same overall log format,
        // should be removed if the format is updated
        gDumpFn("REFTEST TEST-LOAD | " + g.currentURL + " | " + currentTest + " / " + g.totalTests +
                " (" + Math.floor(100 * (currentTest / g.totalTests)) + "%)\n");
        TestBuffer("START " + g.currentURL);
        var type = g.urls[0].type
        if (TYPE_SCRIPT == type) {
            SendLoadScriptTest(g.currentURL, g.loadTimeout);
        } else if (TYPE_PRINT == type) {
            SendLoadPrintTest(g.currentURL, g.loadTimeout);
        } else {
            SendLoadTest(type, g.currentURL, g.currentURLTargetType, g.loadTimeout);
        }
    }
}

function DoneTests()
{
    logger.suiteEnd({'results': g.testResults});
    g.suiteStarted = false
    logger.info("Slowest test took " + g.slowestTestTime + "ms (" + g.slowestTestURL + ")");
    logger.info("Total canvas count = " + g.recycledCanvases.length);
    if (g.failedUseWidgetLayers) {
        LogWidgetLayersFailure();
    }

    function onStopped() {
        let appStartup = CC["@mozilla.org/toolkit/app-startup;1"].getService(CI.nsIAppStartup);
        appStartup.quit(CI.nsIAppStartup.eForceQuit);
    }
    if (g.server) {
        g.server.stop(onStopped);
    }
    else {
        onStopped();
    }
}

function UpdateCanvasCache(url, canvas)
{
    var spec = url.spec;

    --g.uriUseCounts[spec];

    if (g.uriUseCounts[spec] == 0) {
        ReleaseCanvas(canvas);
        delete g.uriCanvases[spec];
    } else if (g.uriUseCounts[spec] > 0) {
        g.uriCanvases[spec] = canvas;
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
    var testRect = g.browser.getBoundingClientRect();
    if (g.ignoreWindowSize ||
        (0 <= testRect.left &&
         0 <= testRect.top &&
         g.containingWindow.innerWidth >= testRect.right &&
         g.containingWindow.innerHeight >= testRect.bottom)) {
        // We can use the window's retained layer manager
        // because the window is big enough to display the entire
        // browser element
        flags |= ctx.DRAWWINDOW_USE_WIDGET_LAYERS;
    } else if (g.browserIsRemote) {
        logger.error(g.currentURL + " | can't drawWindow remote content");
        ++g.testResults.Exception;
    }

    if (g.drawWindowFlags != flags) {
        // Every time the flags change, dump the new state.
        g.drawWindowFlags = flags;
        var flagsStr = "DRAWWINDOW_DRAW_CARET | DRAWWINDOW_DRAW_VIEW";
        if (flags & ctx.DRAWWINDOW_USE_WIDGET_LAYERS) {
            flagsStr += " | DRAWWINDOW_USE_WIDGET_LAYERS";
        } else {
            // Output a special warning because we need to be able to detect
            // this whenever it happens.
            LogWidgetLayersFailure();
            g.failedUseWidgetLayers = true;
        }
        logger.info("drawWindow flags = " + flagsStr +
                    "; window size = " + g.containingWindow.innerWidth + "," + g.containingWindow.innerHeight +
                    "; test browser size = " + testRect.width + "," + testRect.height);
    }

    TestBuffer("DoDrawWindow " + x + "," + y + "," + w + "," + h);
    ctx.drawWindow(g.containingWindow, x, y, w, h, "rgb(255,255,255)",
                   g.drawWindowFlags);
}

function InitCurrentCanvasWithSnapshot()
{
    TestBuffer("Initializing canvas snapshot");

    if (g.urls[0].type == TYPE_LOAD || g.urls[0].type == TYPE_SCRIPT || g.urls[0].type == TYPE_PRINT) {
        // We don't want to snapshot this kind of test
        return false;
    }

    if (!g.currentCanvas) {
        g.currentCanvas = AllocateCanvas();
    }

    var ctx = g.currentCanvas.getContext("2d");
    DoDrawWindow(ctx, 0, 0, g.currentCanvas.width, g.currentCanvas.height);
    return true;
}

function UpdateCurrentCanvasForInvalidation(rects)
{
    TestBuffer("Updating canvas for invalidation");

    if (!g.currentCanvas) {
        return;
    }

    var ctx = g.currentCanvas.getContext("2d");
    for (var i = 0; i < rects.length; ++i) {
        var r = rects[i];
        // Set left/top/right/bottom to pixel boundaries
        var left = Math.floor(r.left);
        var top = Math.floor(r.top);
        var right = Math.ceil(r.right);
        var bottom = Math.ceil(r.bottom);

        // Clamp the values to the canvas size
        left = Math.max(0, Math.min(left, g.currentCanvas.width));
        top = Math.max(0, Math.min(top, g.currentCanvas.height));
        right = Math.max(0, Math.min(right, g.currentCanvas.width));
        bottom = Math.max(0, Math.min(bottom, g.currentCanvas.height));

        ctx.save();
        ctx.translate(left, top);
        DoDrawWindow(ctx, left, top, right - left, bottom - top);
        ctx.restore();
    }
}

function UpdateWholeCurrentCanvasForInvalidation()
{
    TestBuffer("Updating entire canvas for invalidation");

    if (!g.currentCanvas) {
        return;
    }

    var ctx = g.currentCanvas.getContext("2d");
    DoDrawWindow(ctx, 0, 0, g.currentCanvas.width, g.currentCanvas.height);
}

function RecordResult(testRunTime, errorMsg, typeSpecificResults)
{
    TestBuffer("RecordResult fired");

    // Keep track of which test was slowest, and how long it took.
    if (testRunTime > g.slowestTestTime) {
        g.slowestTestTime = testRunTime;
        g.slowestTestURL  = g.currentURL;
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

    if (g.urls[0].type == TYPE_LOAD) {
        ++g.testResults.LoadOnly;
        logger.testStatus(g.urls[0].identifier, "(LOAD ONLY)", "PASS", "PASS");
        g.currentCanvas = null;
        FinishTestItem();
        return;
    }
    if (g.urls[0].type == TYPE_PRINT) {
        switch (g.currentURLTargetType) {
        case URL_TARGET_TYPE_TEST:
            // First document has been loaded.
            g.testPrintOutput = typeSpecificResults;
            // Proceed to load the second document.
            CleanUpCrashDumpFiles();
            StartCurrentURI(URL_TARGET_TYPE_REFERENCE);
            break;
        case URL_TARGET_TYPE_REFERENCE:
            let pathToTestPdf = g.testPrintOutput;
            let pathToRefPdf = typeSpecificResults;
            comparePdfs(pathToTestPdf, pathToRefPdf, function(error, results) {
                let expected = g.urls[0].expected;
                // TODO: We should complain here if results is empty!
                // (If it's empty, we'll spuriously succeed, regardless of
                // our expectations)
                if (error) {
                    output = outputs[expected][false];
                    extra = { status_msg: output.n };
                    ++g.testResults[output.n];
                    logger.testEnd(g.urls[0].identifier, output.s[0], output.s[1],
                                   error.message, null, extra);
                } else {
                    let outputPair = outputs[expected];
                    if (expected === EXPECTED_FAIL) {
                       let failureResults = results.filter(function (result) { return !result.passed });
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
                        let extra = { status_msg: output.n };
                        ++g.testResults[output.n];
                        logger.testEnd(g.urls[0].identifier, output.s[0], output.s[1],
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
    if (g.urls[0].type == TYPE_SCRIPT) {
        var expected = g.urls[0].expected;

        if (errorMsg) {
            // Force an unexpected failure to alert the test author to fix the test.
            expected = EXPECTED_PASS;
        } else if (typeSpecificResults.length == 0) {
             // This failure may be due to a JavaScript Engine bug causing
             // early termination of the test. If we do not allow silent
             // failure, report an error.
             if (!g.urls[0].allowSilentFail)
                 errorMsg = "No test results reported. (SCRIPT)\n";
             else
                 logger.info("An expected silent failure occurred");
        }

        if (errorMsg) {
            output = outputs[expected][false];
            extra = { status_msg: output.n };
            ++g.testResults[output.n];
            logger.testStatus(g.urls[0].identifier, errorMsg, output.s[0], output.s[1], null, null, extra);
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

                ++g.testResults[output.n];
                logger.testStatus(g.urls[0].identifier, result.description + " item " + (++index),
                                  output.s[0], output.s[1], null, null, extra);
            });

        if (anyFailed && expected == EXPECTED_PASS) {
            FlushTestBuffer();
        }

        FinishTestItem();
        return;
    }

    const isRecordingRef =
      (g.currentURLTargetType == URL_TARGET_TYPE_REFERENCE);
    const prefSettings =
      g.urls[0][isRecordingRef ? "prefSettings2" : "prefSettings1"];

    if (prefSettings.length == 0 && g.uriCanvases[g.currentURL]) {
        g.currentCanvas = g.uriCanvases[g.currentURL];
    }
    if (g.currentCanvas == null) {
        logger.error(g.currentURL, "program error managing snapshots");
        ++g.testResults.Exception;
    }
    g[isRecordingRef ? "canvas2" : "canvas1"] = g.currentCanvas;
    g.currentCanvas = null;

    ResetRenderingState();

    switch (g.currentURLTargetType) {
        case URL_TARGET_TYPE_TEST:
            // First document has been loaded.
            // Proceed to load the second document.

            CleanUpCrashDumpFiles();
            StartCurrentURI(URL_TARGET_TYPE_REFERENCE);
            break;
        case URL_TARGET_TYPE_REFERENCE:
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

            differences = g.windowUtils.compareCanvases(g.canvas1, g.canvas2, maxDifference);
            equal = (differences == 0);

            if (maxDifference.value > 0 && equal) {
                throw "Inconsistent result from compareCanvases.";
            }

            // what is expected on this platform (PASS, FAIL, or RANDOM)
            var expected = g.urls[0].expected;

            if (expected == EXPECTED_FUZZY) {
                logger.info(`REFTEST fuzzy test ` +
                            `(${g.urls[0].fuzzyMinDelta}, ${g.urls[0].fuzzyMinPixels}) <= ` +
                            `(${maxDifference.value}, ${differences}) <= ` +
                            `(${g.urls[0].fuzzyMaxDelta}, ${g.urls[0].fuzzyMaxPixels})`);
                fuzz_exceeded = maxDifference.value > g.urls[0].fuzzyMaxDelta ||
                                differences > g.urls[0].fuzzyMaxPixels;
                equal = !fuzz_exceeded &&
                        maxDifference.value >= g.urls[0].fuzzyMinDelta &&
                        differences >= g.urls[0].fuzzyMinPixels;
            }

            var failedExtraCheck = g.failedNoPaint || g.failedNoDisplayList || g.failedDisplayList || g.failedOpaqueLayer || g.failedAssignedLayer;

            // whether the comparison result matches what is in the manifest
            var test_passed = (equal == (g.urls[0].type == TYPE_REFTEST_EQUAL)) && !failedExtraCheck;

            if (expected != EXPECTED_FUZZY) {
                output = outputs[expected][test_passed];
            } else if (test_passed) {
                output = {s: ["PASS", "PASS"], n: "Pass"};
            } else if (g.urls[0].type == TYPE_REFTEST_EQUAL &&
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

            ++g.testResults[output.n];

            // It's possible that we failed both an "extra check" and the normal comparison, but we don't
            // have a way to annotate these separately, so just print an error for the extra check failures.
            if (failedExtraCheck) {
                var failures = [];
                if (g.failedNoPaint) {
                    failures.push("failed reftest-no-paint");
                }
                if (g.failedNoDisplayList) {
                    failures.push("failed reftest-no-display-list");
                }
                if (g.failedDisplayList) {
                    failures.push("failed reftest-display-list");
                }
                // The g.failed*Messages arrays will contain messages from both the test and the reference.
                if (g.failedOpaqueLayer) {
                    failures.push("failed reftest-opaque-layer: " + g.failedOpaqueLayerMessages.join(", "));
                }
                if (g.failedAssignedLayer) {
                    failures.push("failed reftest-assigned-layer: " + g.failedAssignedLayerMessages.join(", "));
                }
                var failureString = failures.join(", ");
                logger.testStatus(g.urls[0].identifier, failureString, output.s[0], output.s[1], null, null, extra);
            } else {
                var message = "image comparison, max difference: " + maxDifference.value +
                              ", number of differing pixels: " + differences;
                if (!test_passed && expected == EXPECTED_PASS ||
                    !test_passed && expected == EXPECTED_FUZZY ||
                    test_passed && expected == EXPECTED_FAIL) {
                    if (!equal) {
                        extra.max_difference = maxDifference.value;
                        extra.differences = differences;
                        var image1 = g.canvas1.toDataURL();
                        var image2 = g.canvas2.toDataURL();
                        extra.reftest_screenshots = [
                            {url:g.urls[0].identifier[0],
                             screenshot: image1.slice(image1.indexOf(",") + 1)},
                            g.urls[0].identifier[1],
                            {url:g.urls[0].identifier[2],
                             screenshot: image2.slice(image2.indexOf(",") + 1)}
                        ];
                        extra.image1 = image1;
                        extra.image2 = image2;
                    } else {
                        var image1 = g.canvas1.toDataURL();
                        extra.reftest_screenshots = [
                            {url:g.urls[0].identifier[0],
                             screenshot: image1.slice(image1.indexOf(",") + 1)}
                        ];
                        extra.image1 = image1;
                    }
                }
                logger.testStatus(g.urls[0].identifier, message, output.s[0], output.s[1], null, null, extra);

                if (g.noCanvasCache) {
                    ReleaseCanvas(g.canvas1);
                    ReleaseCanvas(g.canvas2);
                } else {
                    if (g.urls[0].prefSettings1.length == 0) {
                        UpdateCanvasCache(g.urls[0].url1, g.canvas1);
                    }
                    if (g.urls[0].prefSettings2.length == 0) {
                        UpdateCanvasCache(g.urls[0].url2, g.canvas2);
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
    ++g.testResults.FailedLoad;
    if (!why) {
        // reftest-content.js sets an initial reason before it sets the
        // timeout that will call us with the currently set reason, so we
        // should never get here.  If we do then there's a logic error
        // somewhere.  Perhaps tests are somehow running overlapped and the
        // timeout for one test is not being cleared before the timeout for
        // another is set?  Maybe there's some sort of race?
        logger.error("load failed with unknown reason (we should always have a reason!)");
    }
    logger.testStatus(g.urls[0].identifier, "load failed: " + why, "FAIL", "PASS");
    FlushTestBuffer();
    FinishTestItem();
}

function RemoveExpectedCrashDumpFiles()
{
    if (g.expectingProcessCrash) {
        for (let crashFilename of g.expectedCrashDumpFiles) {
            let file = g.crashDumpDir.clone();
            file.append(crashFilename);
            if (file.exists()) {
                file.remove(false);
            }
        }
    }
    g.expectedCrashDumpFiles.length = 0;
}

function FindUnexpectedCrashDumpFiles()
{
    if (!g.crashDumpDir.exists()) {
        return;
    }

    let entries = g.crashDumpDir.directoryEntries;
    if (!entries) {
        return;
    }

    let foundCrashDumpFile = false;
    while (entries.hasMoreElements()) {
        let file = entries.getNext().QueryInterface(CI.nsIFile);
        let path = String(file.path);
        if (path.match(/\.(dmp|extra)$/) && !g.unexpectedCrashDumpFiles[path]) {
            if (!foundCrashDumpFile) {
                ++g.testResults.UnexpectedFail;
                foundCrashDumpFile = true;
                if (g.currentURL) {
                    logger.testStatus(g.urls[0].identifier, "crash-check", "FAIL", "PASS", "This test left crash dumps behind, but we weren't expecting it to!");
                } else {
                    logger.error("Harness startup left crash dumps behind, but we weren't expecting it to!");
                }
            }
            logger.info("Found unexpected crash dump file " + path);
            g.unexpectedCrashDumpFiles[path] = true;
        }
    }
}

function RemovePendingCrashDumpFiles()
{
    if (!g.pendingCrashDumpDir.exists()) {
        return;
    }

    let entries = g.pendingCrashDumpDir.directoryEntries;
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
    if (g.cleanupPendingCrashes) {
      RemovePendingCrashDumpFiles();
    }
    g.expectingProcessCrash = false;
}

function FinishTestItem()
{
    logger.testEnd(g.urls[0].identifier, "OK");

    // Replace document with BLANK_URL_FOR_CLEARING in case there are
    // assertions when unloading.
    logger.debug("Loading a blank page");
    // After clearing, content will notify us of the assertion count
    // and tests will continue.
    SendClear();
    g.failedNoPaint = false;
    g.failedNoDisplayList = false;
    g.failedDisplayList = false;
    g.failedOpaqueLayer = false;
    g.failedOpaqueLayerMessages = [];
    g.failedAssignedLayer = false;
    g.failedAssignedLayerMessages = [];
}

function DoAssertionCheck(numAsserts)
{
    if (g.debug.isDebugBuild) {
        if (g.browserIsRemote) {
            // Count chrome-process asserts too when content is out of
            // process.
            var newAssertionCount = g.debug.assertionCount;
            var numLocalAsserts = newAssertionCount - g.assertionCount;
            g.assertionCount = newAssertionCount;

            numAsserts += numLocalAsserts;
        }

        var minAsserts = g.urls[0].minAsserts;
        var maxAsserts = g.urls[0].maxAsserts;

        logger.assertionCount(g.urls[0].identifier, numAsserts, minAsserts, maxAsserts);
    }

    if (g.urls[0].chaosMode) {
        g.windowUtils.leaveChaosMode();
    }

    // And start the next test.
    g.urls.shift();
    StartCurrentTest();
}

function ResetRenderingState()
{
    SendResetRenderingState();
    // We would want to clear any viewconfig here, if we add support for it
}

function RestoreChangedPreferences()
{
    if (g.prefsToRestore.length > 0) {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                    getService(Components.interfaces.nsIPrefBranch);
        g.prefsToRestore.reverse();
        g.prefsToRestore.forEach(function(ps) {
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
        g.prefsToRestore = [];
    }
}

function RegisterMessageListenersAndLoadContentScript()
{
    g.browserMessageManager.addMessageListener(
        "reftest:AssertionCount",
        function (m) { RecvAssertionCount(m.json.count); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:ContentReady",
        function (m) { return RecvContentReady(m.data); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:Exception",
        function (m) { RecvException(m.json.what) }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:FailedLoad",
        function (m) { RecvFailedLoad(m.json.why); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:FailedNoPaint",
        function (m) { RecvFailedNoPaint(); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:FailedNoDisplayList",
        function (m) { RecvFailedNoDisplayList(); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:FailedDisplayList",
        function (m) { RecvFailedDisplayList(); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:FailedOpaqueLayer",
        function (m) { RecvFailedOpaqueLayer(m.json.why); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:FailedAssignedLayer",
        function (m) { RecvFailedAssignedLayer(m.json.why); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:InitCanvasWithSnapshot",
        function (m) { return RecvInitCanvasWithSnapshot(); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:Log",
        function (m) { RecvLog(m.json.type, m.json.msg); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:ScriptResults",
        function (m) { RecvScriptResults(m.json.runtimeMs, m.json.error, m.json.results); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:PrintResult",
        function (m) { RecvPrintResult(m.json.runtimeMs, m.json.status, m.json.fileName); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:TestDone",
        function (m) { RecvTestDone(m.json.runtimeMs); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:UpdateCanvasForInvalidation",
        function (m) { RecvUpdateCanvasForInvalidation(m.json.rects); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:UpdateWholeCanvasForInvalidation",
        function (m) { RecvUpdateWholeCanvasForInvalidation(); }
    );
    g.browserMessageManager.addMessageListener(
        "reftest:ExpectProcessCrash",
        function (m) { RecvExpectProcessCrash(); }
    );

    g.browserMessageManager.loadFrameScript("chrome://reftest/content/reftest-content.js", true, true);
}

function RecvAssertionCount(count)
{
    DoAssertionCheck(count);
}

function RecvContentReady(info)
{
    g.contentGfxInfo = info.gfx;
    InitAndStartRefTests();
    return { remote: g.browserIsRemote };
}

function RecvException(what)
{
    logger.error(g.currentURL + " | " + what);
    ++g.testResults.Exception;
}

function RecvFailedLoad(why)
{
    LoadFailed(why);
}

function RecvFailedNoPaint()
{
    g.failedNoPaint = true;
}

function RecvFailedNoDisplayList()
{
    g.failedNoDisplayList = true;
}

function RecvFailedDisplayList()
{
    g.failedDisplayList = true;
}

function RecvFailedOpaqueLayer(why) {
    g.failedOpaqueLayer = true;
    g.failedOpaqueLayerMessages.push(why);
}

function RecvFailedAssignedLayer(why) {
    g.failedAssignedLayer = true;
    g.failedAssignedLayerMessages.push(why);
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
        logger.error("REFTEST TEST-UNEXPECTED-FAIL | " + g.currentURL + " | unknown log type " + type + "\n");
        ++g.testResults.Exception;
    }
}

function RecvScriptResults(runtimeMs, error, results)
{
    RecordResult(runtimeMs, error, results);
}

function RecvPrintResult(runtimeMs, status, fileName)
{
    if (!Components.isSuccessCode(status)) {
      logger.error("REFTEST TEST-UNEXPECTED-FAIL | " + g.currentURL + " | error during printing\n");
      ++g.testResults.Exception;
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
        g.expectedCrashDumpFiles.push(id + ".dmp");
        g.expectedCrashDumpFiles.push(id + ".extra");
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
    g.expectingProcessCrash = true;
}

function SendClear()
{
    g.browserMessageManager.sendAsyncMessage("reftest:Clear");
}

function SendLoadScriptTest(uri, timeout)
{
    g.browserMessageManager.sendAsyncMessage("reftest:LoadScriptTest",
                                            { uri: uri, timeout: timeout });
}

function SendLoadPrintTest(uri, timeout)
{
    g.browserMessageManager.sendAsyncMessage("reftest:LoadPrintTest",
                                            { uri: uri, timeout: timeout });
}

function SendLoadTest(type, uri, uriTargetType, timeout)
{
    g.browserMessageManager.sendAsyncMessage("reftest:LoadTest",
                                            { type: type, uri: uri,
                                              uriTargetType: uriTargetType,
                                              timeout: timeout }
    );
}

function SendResetRenderingState()
{
    g.browserMessageManager.sendAsyncMessage("reftest:ResetRenderingState");
}

function readPdf(path, callback) {
    OS.File.open(path, { read: true }).then(function (file) {
        file.flush().then(function() {
            file.read().then(function (data) {
                let fakePort = new PDFJS.main.LoopbackPort(true);
                PDFJS.worker.WorkerMessageHandler.initializeFromPort(fakePort);
                let myWorker = new PDFJS.main.PDFWorker("worker", fakePort);
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
        let numberOfPages = pdfs[1].numPages;
        let sameNumberOfPages = numberOfPages === pdfs[0].numPages;

        let resultPromises = [Promise.resolve({
            passed: sameNumberOfPages,
            description: "Expected number of pages: " + numberOfPages +
                                             ", got " + pdfs[0].numPages
        })];

        if (sameNumberOfPages) {
            for (let i = 0; i < numberOfPages; i++) {
                let pageNum = i + 1;
                let testPagePromise = pdfs[0].getPage(pageNum);
                let refPagePromise = pdfs[1].getPage(pageNum);
                resultPromises.push(new Promise(function(resolve, reject) {
                    Promise.all([testPagePromise, refPagePromise]).then(function(pages) {
                        let testTextPromise = pages[0].getTextContent();
                        let refTextPromise = pages[1].getTextContent();
                        Promise.all([testTextPromise, refTextPromise]).then(function(texts) {
                            let testTextItems = texts[0].items;
                            let refTextItems = texts[1].items;
                            let testText;
                            let refText;
                            let passed = refTextItems.every(function(o, i) {
                                refText = o.str;
                                if (!testTextItems[i]) {
                                    return false;
                                }
                                testText = testTextItems[i].str;
                                return testText === refText;
                            });
                            let description;
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
