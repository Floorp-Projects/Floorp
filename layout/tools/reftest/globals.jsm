/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = [];

for (let [key, val] of Object.entries({
  /* Constants */
  XHTML_NS: "http://www.w3.org/1999/xhtml",
  XUL_NS: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",

  NS_LOCAL_FILE_CONTRACTID: "@mozilla.org/file/local;1",
  NS_GFXINFO_CONTRACTID: "@mozilla.org/gfx/info;1",
  IO_SERVICE_CONTRACTID: "@mozilla.org/network/io-service;1",
  DEBUG_CONTRACTID: "@mozilla.org/xpcom/debug;1",
  NS_DIRECTORY_SERVICE_CONTRACTID: "@mozilla.org/file/directory_service;1",
  NS_OBSERVER_SERVICE_CONTRACTID: "@mozilla.org/observer-service;1",

  TYPE_REFTEST_EQUAL: '==',
  TYPE_REFTEST_NOTEQUAL: '!=',
  TYPE_LOAD: 'load',     // test without a reference (just test that it does
                         // not assert, crash, hang, or leak)
  TYPE_SCRIPT: 'script', // test contains individual test results
  TYPE_PRINT: 'print',   // test and reference will be printed to PDF's and
                         // compared structurally

  // keep this in sync with reftest-content.js
  URL_TARGET_TYPE_TEST: 0,      // first url
  URL_TARGET_TYPE_REFERENCE: 1, // second url, if any

  // The order of these constants matters, since when we have a status
  // listed for a *manifest*, we combine the status with the status for
  // the test by using the *larger*.
  // FIXME: In the future, we may also want to use this rule for combining
  // statuses that are on the same line (rather than making the last one
  // win).
  EXPECTED_PASS: 0,
  EXPECTED_FAIL: 1,
  EXPECTED_RANDOM: 2,
  EXPECTED_FUZZY: 3,

  // types of preference value we might want to set for a specific test
  PREF_BOOLEAN: 0,
  PREF_STRING: 1,
  PREF_INTEGER: 2,

  FOCUS_FILTER_ALL_TESTS: "all",
  FOCUS_FILTER_NEEDS_FOCUS_TESTS: "needs-focus",
  FOCUS_FILTER_NON_NEEDS_FOCUS_TESTS: "non-needs-focus",

  // "<!--CLEAR-->"
  BLANK_URL_FOR_CLEARING: "data:text/html;charset=UTF-8,%3C%21%2D%2DCLEAR%2D%2D%3E",

  /* Globals */
  g: {
    loadTimeout: 0,
    timeoutHook: null,
    remote: false,
    ignoreWindowSize: false,
    shuffle: false,
    repeat: null,
    runUntilFailure: false,
    cleanupPendingCrashes: false,
    totalChunks: 0,
    thisChunk: 0,
    containingWindow: null,
    urlFilterRegex: {},
    contentGfxInfo: null,
    focusFilterMode: "all",
    compareRetainedDisplayLists: false,
    isCoverageBuild: false,

    browser: undefined,
    // Are we testing web content loaded in a separate process?
    browserIsRemote: undefined,        // bool
    // Are we using <iframe mozbrowser>?
    browserIsIframe: undefined,        // bool
    browserMessageManager: undefined,  // bool
    useDrawSnapshot: undefined,        // bool
    canvas1: undefined,
    canvas2: undefined,
    // gCurrentCanvas is non-null between InitCurrentCanvasWithSnapshot and the next
    // RecordResult.
    currentCanvas: null,
    urls: undefined,
    // Map from URI spec to the number of times it remains to be used
    uriUseCounts: undefined,
    // Map from URI spec to the canvas rendered for that URI
    uriCanvases: undefined,
    testResults: {
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
    },
    totalTests: 0,
    currentURL: undefined,
    currentURLTargetType: undefined,
    testLog: [],
    logLevel: undefined,
    logFile: null,
    logger: undefined,
    server: undefined,
    count: 0,
    assertionCount: 0,

    ioService: undefined,
    debug: undefined,
    windowUtils: undefined,

    slowestTestTime: 0,
    slowestTestURL: undefined,
    failedUseWidgetLayers: false,

    drawWindowFlags: undefined,

    expectingProcessCrash: false,
    expectedCrashDumpFiles: [],
    unexpectedCrashDumpFiles: {},
    crashDumpDir: undefined,
    pendingCrashDumpDir: undefined,
    failedNoPaint: false,
    failedNoDisplayList: false,
    failedDisplayList: false,
    failedOpaqueLayer: false,
    failedOpaqueLayerMessages: [],
    failedAssignedLayer: false,
    failedAssignedLayerMessages: [],

    startAfter: undefined,
    suiteStarted: false,
    manageSuite: false,

    prefsToRestore: [],
    httpServerPort: -1,

    // whether to run slow tests or not
    runSlowTests: true,

    // whether we should skip caching canvases
    noCanvasCache: false,
    recycledCanvases: new Array(),
    testPrintOutput: null,

    manifestsLoaded: {},
    // Only dump the sandbox once, because it doesn't depend on the
    // manifest URL (yet!).
    dumpedConditionSandbox: false,
  }
})) {
  this[key] = val;
  EXPORTED_SYMBOLS.push(key);
}
