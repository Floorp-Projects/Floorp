/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CC = Components.classes;
const CI = Components.interfaces;
const CR = Components.results;

/**
 * FIXME/bug 622224: work around lack of reliable setTimeout available
 * to frame scripts.
 */
// This gives us >=2^30 unique timer IDs, enough for 1 per ms for 12.4
// days.  Should be fine as a temporary workaround.
var gNextTimeoutId = 0;
var gTimeoutTable = { };        // int -> nsITimer

function setTimeout(callbackFn, delayMs) {
    var id = gNextTimeoutId++;
    var timer = CC["@mozilla.org/timer;1"].createInstance(CI.nsITimer);
    timer.initWithCallback({
        notify: function notify_callback() {
                    clearTimeout(id);
                    callbackFn();
                }
        },
        delayMs,
        timer.TYPE_ONE_SHOT);

    gTimeoutTable[id] = timer;

    return id;
}

function clearTimeout(id) {
    var timer = gTimeoutTable[id];
    if (timer) {
        timer.cancel();
        delete gTimeoutTable[id];
    }
}

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const DEBUG_CONTRACTID = "@mozilla.org/xpcom/debug;1";
const PRINTSETTINGS_CONTRACTID = "@mozilla.org/gfx/printsettings-service;1";

// "<!--CLEAR-->"
const BLANK_URL_FOR_CLEARING = "data:text/html,%3C%21%2D%2DCLEAR%2D%2D%3E";

var gBrowserIsRemote;
var gHaveCanvasSnapshot = false;
// Plugin layers can be updated asynchronously, so to make sure that all
// layer surfaces have the right content, we need to listen for explicit
// "MozPaintWait" and "MozPaintWaitFinished" events that signal when it's OK
// to take snapshots. We cannot take a snapshot while the number of
// "MozPaintWait" events fired exceeds the number of "MozPaintWaitFinished"
// events fired. We count the number of such excess events here. When
// the counter reaches zero we call gExplicitPendingPaintsCompleteHook.
var gExplicitPendingPaintCount = 0;
var gExplicitPendingPaintsCompleteHook;
var gCurrentURL;
var gCurrentTestType;
var gTimeoutHook = null;
var gFailureTimeout = null;
var gFailureReason;
var gAssertionCount = 0;

var gDebug;

var gCurrentTestStartTime;
var gClearingForAssertionCheck = false;

const TYPE_SCRIPT = 'script'; // test contains individual test results

function markupDocumentViewer() { 
    return docShell.contentViewer.QueryInterface(CI.nsIMarkupDocumentViewer);
}

function webNavigation() {
    return docShell.QueryInterface(CI.nsIWebNavigation);
}

function windowUtils() {
    return content.QueryInterface(CI.nsIInterfaceRequestor)
                  .getInterface(CI.nsIDOMWindowUtils);
}

function IDForEventTarget(event)
{
    try {
        return "'" + event.target.getAttribute('id') + "'";
    } catch (ex) {
        return "<unknown>";
    }
}

function PaintWaitListener(event)
{
    LogInfo("MozPaintWait received for ID " + IDForEventTarget(event));
    gExplicitPendingPaintCount++;
}

function PaintWaitFinishedListener(event)
{
    LogInfo("MozPaintWaitFinished received for ID " + IDForEventTarget(event));
    gExplicitPendingPaintCount--;
    if (gExplicitPendingPaintCount < 0) {
        LogWarning("Underrun in gExplicitPendingPaintCount\n");
        gExplicitPendingPaintCount = 0;
    }
    if (gExplicitPendingPaintCount == 0 &&
        gExplicitPendingPaintsCompleteHook) {
        gExplicitPendingPaintsCompleteHook();
    }
}

function OnInitialLoad()
{
    removeEventListener("load", OnInitialLoad, true);

    gDebug = CC[DEBUG_CONTRACTID].getService(CI.nsIDebug2);

    RegisterMessageListeners();

    var initInfo = SendContentReady();
    gBrowserIsRemote = initInfo.remote;

    addEventListener("load", OnDocumentLoad, true);

    addEventListener("MozPaintWait", PaintWaitListener, true);
    addEventListener("MozPaintWaitFinished", PaintWaitFinishedListener, true);
 
    LogWarning("Using browser remote="+ gBrowserIsRemote +"\n");
}

function StartTestURI(type, uri, timeout)
{
    // Reset gExplicitPendingPaintCount in case there was a timeout or
    // the count is out of sync for some other reason
    if (gExplicitPendingPaintCount != 0) {
        LogWarning("Resetting gExplicitPendingPaintCount to zero (currently " +
                   gExplicitPendingPaintCount + "\n");
        gExplicitPendingPaintCount = 0;
    }

    gCurrentTestType = type;
    gCurrentURL = uri;

    gCurrentTestStartTime = Date.now();
    if (gFailureTimeout != null) {
        SendException("program error managing timeouts\n");
    }
    gFailureTimeout = setTimeout(LoadFailed, timeout);

    LoadURI(gCurrentURL);
}

function setupZoom(contentRootElement) {
    if (!contentRootElement || !contentRootElement.hasAttribute('reftest-zoom'))
        return;
    markupDocumentViewer().fullZoom =
        contentRootElement.getAttribute('reftest-zoom');
}

function resetZoom() {
    markupDocumentViewer().fullZoom = 1.0;
}

function doPrintMode(contentRootElement) {
    // use getAttribute because className works differently in HTML and SVG
    return contentRootElement &&
           contentRootElement.hasAttribute('class') &&
           contentRootElement.getAttribute('class').split(/\s+/)
                             .indexOf("reftest-print") != -1;
}

function setupPrintMode() {
   var PSSVC =
       CC[PRINTSETTINGS_CONTRACTID].getService(CI.nsIPrintSettingsService);
   var ps = PSSVC.newPrintSettings;
   ps.paperWidth = 5;
   ps.paperHeight = 3;

   // Override any os-specific unwriteable margins
   ps.unwriteableMarginTop = 0;
   ps.unwriteableMarginLeft = 0;
   ps.unwriteableMarginBottom = 0;
   ps.unwriteableMarginRight = 0;

   ps.headerStrLeft = "";
   ps.headerStrCenter = "";
   ps.headerStrRight = "";
   ps.footerStrLeft = "";
   ps.footerStrCenter = "";
   ps.footerStrRight = "";
   docShell.contentViewer.setPageMode(true, ps);
}

function setupDisplayport(contentRootElement) {
    if (!contentRootElement) {
        return;
    }

    function attrOrDefault(attr, def) {
        return contentRootElement.hasAttribute(attr) ?
            contentRootElement.getAttribute(attr) : def;
    }

    var vw = attrOrDefault("reftest-viewport-w", 0);
    var vh = attrOrDefault("reftest-viewport-h", 0);
    if (vw !== 0 || vh !== 0) {
        LogInfo("Setting viewport to <w="+ vw +", h="+ vh +">");
        windowUtils().setCSSViewport(vw, vh);
    }

    // XXX support displayPortX/Y when needed
    var dpw = attrOrDefault("reftest-displayport-w", 0);
    var dph = attrOrDefault("reftest-displayport-h", 0);
    var dpx = attrOrDefault("reftest-displayport-x", 0);
    var dpy = attrOrDefault("reftest-displayport-y", 0);
    if (dpw !== 0 || dph !== 0) {
        LogInfo("Setting displayport to <x="+ dpx +", y="+ dpy +", w="+ dpw +", h="+ dph +">");
        windowUtils().setDisplayPortForElement(dpx, dpy, dpw, dph, content.document.documentElement);
    }
    var asyncScroll = attrOrDefault("reftest-async-scroll", false);
    if (asyncScroll) {
      SendEnableAsyncScroll();
    }

    // XXX support resolution when needed

    // XXX support viewconfig when needed
}

function resetDisplayport() {
    // XXX currently the displayport configuration lives on the
    // presshell and so is "reset" on nav when we get a new presshell.
}

function shouldWaitForExplicitPaintWaiters() {
    return gExplicitPendingPaintCount > 0;
}

function shouldWaitForPendingPaints() {
    // if gHaveCanvasSnapshot is false, we're not taking snapshots so
    // there is no need to wait for pending paints to be flushed.
    return gHaveCanvasSnapshot && windowUtils().isMozAfterPaintPending;
}

function shouldWaitForReftestWaitRemoval(contentRootElement) {
    // use getAttribute because className works differently in HTML and SVG
    return contentRootElement &&
           contentRootElement.hasAttribute('class') &&
           contentRootElement.getAttribute('class').split(/\s+/)
                             .indexOf("reftest-wait") != -1;
}

// Initial state. When the document has loaded and all MozAfterPaint events and
// all explicit paint waits are flushed, we can fire the MozReftestInvalidate
// event and move to the next state.
const STATE_WAITING_TO_FIRE_INVALIDATE_EVENT = 0;
// When reftest-wait has been removed from the root element, we can move to the
// next state.
const STATE_WAITING_FOR_REFTEST_WAIT_REMOVAL = 1;
// When all MozAfterPaint events and all explicit paint waits are flushed, we're
// done and can move to the COMPLETED state.
const STATE_WAITING_TO_FINISH = 2;
const STATE_COMPLETED = 3;

function WaitForTestEnd(contentRootElement, inPrintMode) {
    var stopAfterPaintReceived = false;
    var currentDoc = content.document;
    var state = STATE_WAITING_TO_FIRE_INVALIDATE_EVENT;

    function FlushRendering() {
        var anyPendingPaintsGeneratedInDescendants = false;

        function flushWindow(win) {
            var utils = win.QueryInterface(CI.nsIInterfaceRequestor)
                        .getInterface(CI.nsIDOMWindowUtils);
            var afterPaintWasPending = utils.isMozAfterPaintPending;

            try {
                // Flush pending restyles and reflows for this window
                win.document.documentElement.getBoundingClientRect();
            } catch (e) {
                LogWarning("flushWindow failed: " + e + "\n");
            }
            
            if (!afterPaintWasPending && utils.isMozAfterPaintPending) {
                LogInfo("FlushRendering generated paint for window " + win.location.href);
                anyPendingPaintsGeneratedInDescendants = true;
            }

            for (var i = 0; i < win.frames.length; ++i) {
                flushWindow(win.frames[i]);
            }
        }

        flushWindow(content);

        if (anyPendingPaintsGeneratedInDescendants &&
            !windowUtils().isMozAfterPaintPending) {
            LogWarning("Internal error: descendant frame generated a MozAfterPaint event, but the root document doesn't have one!");
        }
    }

    function AfterPaintListener(event) {
        LogInfo("AfterPaintListener in " + event.target.document.location.href);
        if (event.target.document != currentDoc) {
            // ignore paint events for subframes or old documents in the window.
            // Invalidation in subframes will cause invalidation in the toplevel document anyway.
            return;
        }

        SendUpdateCanvasForEvent(event);
        // These events are fired immediately after a paint. Don't
        // confuse ourselves by firing synchronously if we triggered the
        // paint ourselves.
        setTimeout(MakeProgress, 0);
    }

    function AttrModifiedListener() {
        LogInfo("AttrModifiedListener fired");
        // Wait for the next return-to-event-loop before continuing --- for
        // example, the attribute may have been modified in an subdocument's
        // load event handler, in which case we need load event processing
        // to complete and unsuppress painting before we check isMozAfterPaintPending.
        setTimeout(MakeProgress, 0);
    }

    function ExplicitPaintsCompleteListener() {
        LogInfo("ExplicitPaintsCompleteListener fired");
        // Since this can fire while painting, don't confuse ourselves by
        // firing synchronously. It's fine to do this asynchronously.
        setTimeout(MakeProgress, 0);
    }

    function RemoveListeners() {
        // OK, we can end the test now.
        removeEventListener("MozAfterPaint", AfterPaintListener, false);
        if (contentRootElement) {
            contentRootElement.removeEventListener("DOMAttrModified", AttrModifiedListener, false);
        }
        gExplicitPendingPaintsCompleteHook = null;
        gTimeoutHook = null;
        // Make sure we're in the COMPLETED state just in case
        // (this may be called via the test-timeout hook)
        state = STATE_COMPLETED;
    }

    // Everything that could cause shouldWaitForXXX() to
    // change from returning true to returning false is monitored via some kind
    // of event listener which eventually calls this function.
    function MakeProgress() {
        if (state >= STATE_COMPLETED) {
            LogInfo("MakeProgress: STATE_COMPLETED");
            return;
        }

        FlushRendering();

        switch (state) {
        case STATE_WAITING_TO_FIRE_INVALIDATE_EVENT: {
            LogInfo("MakeProgress: STATE_WAITING_TO_FIRE_INVALIDATE_EVENT");
            if (shouldWaitForExplicitPaintWaiters() || shouldWaitForPendingPaints()) {
                gFailureReason = "timed out waiting for pending paint count to reach zero";
                if (shouldWaitForExplicitPaintWaiters()) {
                    gFailureReason += " (waiting for MozPaintWaitFinished)";
                    LogInfo("MakeProgress: waiting for MozPaintWaitFinished");
                }
                if (shouldWaitForPendingPaints()) {
                    gFailureReason += " (waiting for MozAfterPaint)";
                    LogInfo("MakeProgress: waiting for MozAfterPaint");
                }
                return;
            }

            state = STATE_WAITING_FOR_REFTEST_WAIT_REMOVAL;
            var hasReftestWait = shouldWaitForReftestWaitRemoval(contentRootElement);            
            // Notify the test document that now is a good time to test some invalidation
            LogInfo("MakeProgress: dispatching MozReftestInvalidate");
            if (contentRootElement) {
                var notification = content.document.createEvent("Events");
                notification.initEvent("MozReftestInvalidate", true, false);
                contentRootElement.dispatchEvent(notification);
            }
            if (hasReftestWait && !shouldWaitForReftestWaitRemoval(contentRootElement)) {
                // MozReftestInvalidate handler removed reftest-wait.
                // We expect something to have been invalidated...
                FlushRendering();
                if (!shouldWaitForPendingPaints() && !shouldWaitForExplicitPaintWaiters()) {
                    LogWarning("MozInvalidateEvent didn't invalidate");
                }
            }
            // Try next state
            MakeProgress();
            return;
        }

        case STATE_WAITING_FOR_REFTEST_WAIT_REMOVAL:
            LogInfo("MakeProgress: STATE_WAITING_FOR_REFTEST_WAIT_REMOVAL");
            if (shouldWaitForReftestWaitRemoval(contentRootElement)) {
                gFailureReason = "timed out waiting for reftest-wait to be removed";
                LogInfo("MakeProgress: waiting for reftest-wait to be removed");
                return;
            }
            state = STATE_WAITING_TO_FINISH;
            if (!inPrintMode && doPrintMode(contentRootElement)) {
                LogInfo("MakeProgress: setting up print mode");
                setupPrintMode();
            }
            // Try next state
            MakeProgress();
            return;

        case STATE_WAITING_TO_FINISH:
            LogInfo("MakeProgress: STATE_WAITING_TO_FINISH");
            if (shouldWaitForExplicitPaintWaiters() || shouldWaitForPendingPaints()) {
                gFailureReason = "timed out waiting for pending paint count to " +
                    "reach zero (after reftest-wait removed and switch to print mode)";
                if (shouldWaitForExplicitPaintWaiters()) {
                    gFailureReason += " (waiting for MozPaintWaitFinished)";
                    LogInfo("MakeProgress: waiting for MozPaintWaitFinished");
                }
                if (shouldWaitForPendingPaints()) {
                    gFailureReason += " (waiting for MozAfterPaint)";
                    LogInfo("MakeProgress: waiting for MozAfterPaint");
                }
                return;
            }
            LogInfo("MakeProgress: Completed");
            state = STATE_COMPLETED;
            gFailureReason = "timed out while taking snapshot (bug in harness?)";
            RemoveListeners();
            CheckForProcessCrashExpectation();
            setTimeout(RecordResult, 0);
            return;
        }
    }

    LogInfo("WaitForTestEnd: Adding listeners");
    addEventListener("MozAfterPaint", AfterPaintListener, false);
    // If contentRootElement is null then shouldWaitForReftestWaitRemoval will
    // always return false so we don't need a listener anyway
    if (contentRootElement) {
      contentRootElement.addEventListener("DOMAttrModified", AttrModifiedListener, false);
    }
    gExplicitPendingPaintsCompleteHook = ExplicitPaintsCompleteListener;
    gTimeoutHook = RemoveListeners;

    // Take a full snapshot now that all our listeners are set up. This
    // ensures it's impossible for us to miss updates between taking the snapshot
    // and adding our listeners.
    SendInitCanvasWithSnapshot();
    MakeProgress();
}

function OnDocumentLoad(event)
{
    var currentDoc = content.document;
    if (event.target != currentDoc)
        // Ignore load events for subframes.
        return;

    if (gClearingForAssertionCheck &&
        currentDoc.location.href == BLANK_URL_FOR_CLEARING) {
        DoAssertionCheck();
        return;
    }

    if (currentDoc.location.href != gCurrentURL) {
        LogInfo("OnDocumentLoad fired for previous document");
        // Ignore load events for previous documents.
        return;
    }

    var contentRootElement = currentDoc ? currentDoc.documentElement : null;
    currentDoc = null;
    setupZoom(contentRootElement);
    setupDisplayport(contentRootElement);
    var inPrintMode = false;

    function AfterOnLoadScripts() {
        // Regrab the root element, because the document may have changed.
        var contentRootElement =
          content.document ? content.document.documentElement : null;

        // Take a snapshot now. We need to do this before we check whether
        // we should wait, since this might trigger dispatching of
        // MozPaintWait events and make shouldWaitForExplicitPaintWaiters() true
        // below.
        var painted = SendInitCanvasWithSnapshot();

        if (shouldWaitForExplicitPaintWaiters() ||
            (!inPrintMode && doPrintMode(contentRootElement)) ||
            // If we didn't force a paint above, in
            // InitCurrentCanvasWithSnapshot, so we should wait for a
            // paint before we consider them done.
            !painted) {
            LogInfo("AfterOnLoadScripts belatedly entering WaitForTestEnd");
            // Go into reftest-wait mode belatedly.
            WaitForTestEnd(contentRootElement, inPrintMode);
        } else {
            CheckForProcessCrashExpectation();
            RecordResult();
        }
    }

    if (shouldWaitForReftestWaitRemoval(contentRootElement) ||
        shouldWaitForExplicitPaintWaiters()) {
        // Go into reftest-wait mode immediately after painting has been
        // unsuppressed, after the onload event has finished dispatching.
        gFailureReason = "timed out waiting for test to complete (trying to get into WaitForTestEnd)";
        LogInfo("OnDocumentLoad triggering WaitForTestEnd");
        setTimeout(function () { WaitForTestEnd(contentRootElement, inPrintMode); }, 0);
    } else {
        if (doPrintMode(contentRootElement)) {
            LogInfo("OnDocumentLoad setting up print mode");
            setupPrintMode();
            inPrintMode = true;
        }

        // Since we can't use a bubbling-phase load listener from chrome,
        // this is a capturing phase listener.  So do setTimeout twice, the
        // first to get us after the onload has fired in the content, and
        // the second to get us after any setTimeout(foo, 0) in the content.
        gFailureReason = "timed out waiting for test to complete (waiting for onload scripts to complete)";
        LogInfo("OnDocumentLoad triggering AfterOnLoadScripts");
        setTimeout(function () { setTimeout(AfterOnLoadScripts, 0); }, 0);
    }
}

function CheckForProcessCrashExpectation()
{
    var contentRootElement = content.document.documentElement;
    if (contentRootElement &&
        contentRootElement.hasAttribute('class') &&
        contentRootElement.getAttribute('class').split(/\s+/)
                          .indexOf("reftest-expect-process-crash") != -1) {
        SendExpectProcessCrash();
    }
}

function RecordResult()
{
    LogInfo("RecordResult fired");

    var currentTestRunTime = Date.now() - gCurrentTestStartTime;

    clearTimeout(gFailureTimeout);
    gFailureReason = null;
    gFailureTimeout = null;

    if (gCurrentTestType == TYPE_SCRIPT) {
        var error = '';
        var testwindow = content;

        if (testwindow.wrappedJSObject)
            testwindow = testwindow.wrappedJSObject;

        var testcases;
        if (!testwindow.getTestCases || typeof testwindow.getTestCases != "function") {
            // Force an unexpected failure to alert the test author to fix the test.
            error = "test must provide a function getTestCases(). (SCRIPT)\n";
        }
        else if (!(testcases = testwindow.getTestCases())) {
            // Force an unexpected failure to alert the test author to fix the test.
            error = "test's getTestCases() must return an Array-like Object. (SCRIPT)\n";
        }
        else if (testcases.length == 0) {
            // This failure may be due to a JavaScript Engine bug causing
            // early termination of the test. If we do not allow silent
            // failure, the driver will report an error.
        }

        var results = [ ];
        if (!error) {
            // FIXME/bug 618176: temporary workaround
            for (var i = 0; i < testcases.length; ++i) {
                var test = testcases[i];
                results.push({ passed: test.testPassed(),
                               description: test.testDescription() });
            }
            //results = testcases.map(function(test) {
            //        return { passed: test.testPassed(),
            //                 description: test.testDescription() };
        }

        SendScriptResults(currentTestRunTime, error, results);
        FinishTestItem();
        return;
    }

    SendTestDone(currentTestRunTime);
    FinishTestItem();
}

function LoadFailed()
{
    if (gTimeoutHook) {
        gTimeoutHook();
    }
    gFailureTimeout = null;
    SendFailedLoad(gFailureReason);
}

function FinishTestItem()
{
    gHaveCanvasSnapshot = false;
}

function DoAssertionCheck()
{
    gClearingForAssertionCheck = false;

    var numAsserts = 0;
    if (gDebug.isDebugBuild) {
        var newAssertionCount = gDebug.assertionCount;
        numAsserts = newAssertionCount - gAssertionCount;
        gAssertionCount = newAssertionCount;
    }
    SendAssertionCount(numAsserts);
}

function LoadURI(uri)
{
    var flags = webNavigation().LOAD_FLAGS_NONE;
    webNavigation().loadURI(uri, flags, null, null, null);
}

function LogWarning(str)
{
    sendAsyncMessage("reftest:Log", { type: "warning", msg: str });
}

function LogInfo(str)
{
    sendAsyncMessage("reftest:Log", { type: "info", msg: str });
}

const SYNC_DEFAULT = 0x0;
const SYNC_ALLOW_DISABLE = 0x1;
function SynchronizeForSnapshot(flags)
{
    if (flags & SYNC_ALLOW_DISABLE) {
        var docElt = content.document.documentElement;
        if (docElt && docElt.hasAttribute("reftest-no-sync-layers")) {
            LogInfo("Test file chose to skip SynchronizeForSnapshot");
            return;
        }
    }

    var dummyCanvas = content.document.createElementNS(XHTML_NS, "canvas");
    dummyCanvas.setAttribute("width", 1);
    dummyCanvas.setAttribute("height", 1);

    var ctx = dummyCanvas.getContext("2d");
    var flags = ctx.DRAWWINDOW_DRAW_CARET | ctx.DRAWWINDOW_DRAW_VIEW | ctx.DRAWWINDOW_USE_WIDGET_LAYERS;
    ctx.drawWindow(content, 0, 0, 1, 1, "rgb(255,255,255)", flags);
}

function RegisterMessageListeners()
{
    addMessageListener(
        "reftest:Clear",
        function (m) { RecvClear() }
    );
    addMessageListener(
        "reftest:LoadScriptTest",
        function (m) { RecvLoadScriptTest(m.json.uri, m.json.timeout); }
    );
    addMessageListener(
        "reftest:LoadTest",
        function (m) { RecvLoadTest(m.json.type, m.json.uri, m.json.timeout); }
    );
    addMessageListener(
        "reftest:ResetRenderingState",
        function (m) { RecvResetRenderingState(); }
    );
}

function RecvClear()
{
    gClearingForAssertionCheck = true;
    LoadURI(BLANK_URL_FOR_CLEARING);
}

function RecvLoadTest(type, uri, timeout)
{
    StartTestURI(type, uri, timeout);
}

function RecvLoadScriptTest(uri, timeout)
{
    StartTestURI(TYPE_SCRIPT, uri, timeout);
}

function RecvResetRenderingState()
{
    resetZoom();
    resetDisplayport();
}

function SendAssertionCount(numAssertions)
{
    sendAsyncMessage("reftest:AssertionCount", { count: numAssertions });
}

function SendContentReady()
{
    return sendSyncMessage("reftest:ContentReady")[0];
}

function SendException(what)
{
    sendAsyncMessage("reftest:Exception", { what: what });
}

function SendFailedLoad(why)
{
    sendAsyncMessage("reftest:FailedLoad", { why: why });
}

function SendEnableAsyncScroll()
{
    sendAsyncMessage("reftest:EnableAsyncScroll");
}

// Return true if a snapshot was taken.
function SendInitCanvasWithSnapshot()
{
    // If we're in the same process as the top-level XUL window, then
    // drawing that window will also update our layers, so no
    // synchronization is needed.
    //
    // NB: this is a test-harness optimization only, it must not
    // affect the validity of the tests.
    if (gBrowserIsRemote) {
        SynchronizeForSnapshot(SYNC_DEFAULT);
    }

    // For in-process browser, we have to make a synchronous request
    // here to make the above optimization valid, so that MozWaitPaint
    // events dispatched (synchronously) during painting are received
    // before we check the paint-wait counter.  For out-of-process
    // browser though, it doesn't wrt correctness whether this request
    // is sync or async.
    var ret = sendSyncMessage("reftest:InitCanvasWithSnapshot")[0];
 
    gHaveCanvasSnapshot = ret.painted;
    return ret.painted;
}

function SendScriptResults(runtimeMs, error, results)
{
    sendAsyncMessage("reftest:ScriptResults",
                     { runtimeMs: runtimeMs, error: error, results: results });
}

function SendExpectProcessCrash(runtimeMs)
{
    sendAsyncMessage("reftest:ExpectProcessCrash");
}

function SendTestDone(runtimeMs)
{
    sendAsyncMessage("reftest:TestDone", { runtimeMs: runtimeMs });
}

function roundTo(x, fraction)
{
    return Math.round(x/fraction)*fraction;
}

function SendUpdateCanvasForEvent(event)
{
    var win = content;
    var scale = markupDocumentViewer().fullZoom;
 
    var rects = [ ];
    var rectList = event.clientRects;
    for (var i = 0; i < rectList.length; ++i) {
        var r = rectList[i];
        // Set left/top/right/bottom to "device pixel" boundaries
        var left = Math.floor(roundTo(r.left*scale, 0.001));
        var top = Math.floor(roundTo(r.top*scale, 0.001));
        var right = Math.ceil(roundTo(r.right*scale, 0.001));
        var bottom = Math.ceil(roundTo(r.bottom*scale, 0.001));

        rects.push({ left: left, top: top, right: right, bottom: bottom });
    }

    // See comments in SendInitCanvasWithSnapshot() re: the split
    // logic here.
    if (!gBrowserIsRemote) {
        sendSyncMessage("reftest:UpdateCanvasForInvalidation", { rects: rects });
    } else {
        SynchronizeForSnapshot(SYNC_ALLOW_DISABLE);
        sendAsyncMessage("reftest:UpdateCanvasForInvalidation", { rects: rects });
    }
}

addEventListener("load", OnInitialLoad, true);
