/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const DEBUG_CONTRACTID = "@mozilla.org/xpcom/debug;1";
const PRINTSETTINGS_CONTRACTID = "@mozilla.org/gfx/printsettings-service;1";
const ENVIRONMENT_CONTRACTID = "@mozilla.org/process/environment;1";
const NS_OBSERVER_SERVICE_CONTRACTID = "@mozilla.org/observer-service;1";
const NS_GFXINFO_CONTRACTID = "@mozilla.org/gfx/info;1";
const IO_SERVICE_CONTRACTID = "@mozilla.org/network/io-service;1"

// "<!--CLEAR-->"
const BLANK_URL_FOR_CLEARING = "data:text/html;charset=UTF-8,%3C%21%2D%2DCLEAR%2D%2D%3E";

Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://reftest/AsyncSpellCheckTestHelper.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// This will load chrome Custom Elements inside chrome documents:
ChromeUtils.import("resource://gre/modules/CustomElementsListener.jsm", null);

var gBrowserIsRemote;
var gIsWebRenderEnabled;
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
var gCurrentURLRecordResults;
var gCurrentURLTargetType;
var gCurrentTestType;
var gTimeoutHook = null;
var gFailureTimeout = null;
var gFailureReason;
var gAssertionCount = 0;

var gDebug;
var gVerbose = false;

var gCurrentTestStartTime;
var gClearingForAssertionCheck = false;

const TYPE_LOAD = 'load';  // test without a reference (just test that it does
                           // not assert, crash, hang, or leak)
const TYPE_SCRIPT = 'script'; // test contains individual test results
const TYPE_PRINT = 'print'; // test and reference will be printed to PDF's and
                            // compared structurally

// keep this in sync with globals.jsm
const URL_TARGET_TYPE_TEST = 0;      // first url
const URL_TARGET_TYPE_REFERENCE = 1; // second url, if any

function webNavigation() {
    return docShell.QueryInterface(Ci.nsIWebNavigation);
}

function windowUtilsForWindow(w) {
    return w.windowUtils;
}

function windowUtils() {
    return windowUtilsForWindow(content);
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

    gDebug = Cc[DEBUG_CONTRACTID].getService(Ci.nsIDebug2);
    if (gDebug.isDebugBuild) {
        gAssertionCount = gDebug.assertionCount;
    }
    var env = Cc[ENVIRONMENT_CONTRACTID].getService(Ci.nsIEnvironment);
    gVerbose = !!env.get("MOZ_REFTEST_VERBOSE");

    RegisterMessageListeners();

    var initInfo = SendContentReady();
    gBrowserIsRemote = initInfo.remote;

    addEventListener("load", OnDocumentLoad, true);

    addEventListener("MozPaintWait", PaintWaitListener, true);
    addEventListener("MozPaintWaitFinished", PaintWaitFinishedListener, true);

    LogInfo("Using browser remote="+ gBrowserIsRemote +"\n");
}

function SetFailureTimeout(cb, timeout, uri)
{
  var targetTime = Date.now() + timeout;

  var wrapper = function() {
    // Timeouts can fire prematurely in some cases (e.g. in chaos mode). If this
    // happens, set another timeout for the remaining time.
    let remainingMs = targetTime - Date.now();
    if (remainingMs > 0) {
      SetFailureTimeout(cb, remainingMs);
    } else {
      cb();
    }
  }

  // Once OnDocumentLoad is called to handle the 'load' event it will update
  // this error message to reflect what stage of the processing it has reached
  // as it advances to each stage in turn.
  gFailureReason = "timed out after " + timeout +
                   " ms waiting for 'load' event for " + uri;
  gFailureTimeout = setTimeout(wrapper, timeout);
}

function StartTestURI(type, uri, uriTargetType, timeout)
{
    // The GC is only able to clean up compartments after the CC runs. Since
    // the JS ref tests disable the normal browser chrome and do not otherwise
    // create substatial DOM garbage, the CC tends not to run enough normally.
    windowUtils().runNextCollectorTimer();

    // Reset gExplicitPendingPaintCount in case there was a timeout or
    // the count is out of sync for some other reason
    if (gExplicitPendingPaintCount != 0) {
        LogWarning("Resetting gExplicitPendingPaintCount to zero (currently " +
                   gExplicitPendingPaintCount + "\n");
        gExplicitPendingPaintCount = 0;
    }

    gCurrentTestType = type;
    gCurrentURL = uri;
    gCurrentURLTargetType = uriTargetType;
    gCurrentURLRecordResults = 0;

    gCurrentTestStartTime = Date.now();
    if (gFailureTimeout != null) {
        SendException("program error managing timeouts\n");
    }
    SetFailureTimeout(LoadFailed, timeout, uri);

    LoadURI(gCurrentURL);
}

function setupTextZoom(contentRootElement) {
    if (!contentRootElement || !contentRootElement.hasAttribute('reftest-text-zoom'))
        return;
    docShell.browsingContext.textZoom =
        contentRootElement.getAttribute('reftest-text-zoom');
}

function setupFullZoom(contentRootElement) {
    if (!contentRootElement || !contentRootElement.hasAttribute('reftest-zoom'))
        return;
    docShell.browsingContext.fullZoom =
        contentRootElement.getAttribute('reftest-zoom');
}

function resetZoomAndTextZoom() {
    docShell.browsingContext.fullZoom = 1.0;
    docShell.browsingContext.textZoom = 1.0;
}

function doPrintMode(contentRootElement) {
    // use getAttribute because className works differently in HTML and SVG
    if (contentRootElement &&
        contentRootElement.hasAttribute('class')) {
        var classList = contentRootElement.getAttribute('class').split(/\s+/);
        if (classList.includes("reftest-print")) {
            SendException("reftest-print is obsolete, use reftest-paged instead");
            return;
        }
        return classList.includes("reftest-paged");
    }
}

function setupPrintMode() {
   var PSSVC =
       Cc[PRINTSETTINGS_CONTRACTID].getService(Ci.nsIPrintSettingsService);
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
   docShell.contentViewer.setPageModeForTesting(/* aPageMode */ true, ps);
}

// Prints current page to a PDF file and calls callback when sucessfully
// printed and written.
function printToPdf(callback) {
    let currentDoc = content.document;
    let isPrintSelection = false;
    let printRange = '';

    if (currentDoc) {
        let contentRootElement = currentDoc.documentElement;
        printRange = contentRootElement.getAttribute("reftest-print-range") || '';
    }

    if (printRange) {
        if (printRange === 'selection') {
            isPrintSelection = true;
        } else if (!/^[1-9]\d*-[1-9]\d*$/.test(printRange)) {
            SendException("invalid value for reftest-print-range");
            return;
        }
    }

    let fileName = "reftest-print.pdf";
    let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
    file.append(fileName);
    file.createUnique(file.NORMAL_FILE_TYPE, 0o644);

    let PSSVC = Cc[PRINTSETTINGS_CONTRACTID].getService(Ci.nsIPrintSettingsService);
    let ps = PSSVC.newPrintSettings;
    ps.printSilent = true;
    ps.showPrintProgress = false;
    ps.printBGImages = true;
    ps.printBGColors = true;
    ps.printToFile = true;
    ps.toFileName = file.path;
    ps.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;

    if (isPrintSelection) {
        ps.printRange = Ci.nsIPrintSettings.kRangeSelection;
    } else if (printRange) {
        ps.printRange = Ci.nsIPrintSettings.kRangeSpecifiedPageRange;
        let range = printRange.split('-');
        ps.startPageRange = +range[0] || 1;
        ps.endPageRange = +range[1] || 1;
    }

    let webBrowserPrint = content.getInterface(Ci.nsIWebBrowserPrint);
    webBrowserPrint.print(ps, {
        onStateChange: function(webProgress, request, stateFlags, status) {
            if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
                stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
                callback(status, file.path);
            }
        },
        onProgressChange: function () {},
        onLocationChange: function () {},
        onStatusChange: function () {},
        onSecurityChange: function () {},
        onContentBlockingEvent: function () {},
    });
}

function attrOrDefault(element, attr, def) {
    return element.hasAttribute(attr) ? Number(element.getAttribute(attr)) : def;
}

function setupViewport(contentRootElement) {
    if (!contentRootElement) {
        return;
    }

    var sw = attrOrDefault(contentRootElement, "reftest-scrollport-w", 0);
    var sh = attrOrDefault(contentRootElement, "reftest-scrollport-h", 0);
    if (sw !== 0 || sh !== 0) {
        LogInfo("Setting viewport to <w=" + sw + ", h=" + sh + ">");
        windowUtils().setVisualViewportSize(sw, sh);
    }

    var res = attrOrDefault(contentRootElement, "reftest-resolution", 1);
    if (res !== 1) {
        LogInfo("Setting resolution to " + res);
        windowUtils().setResolutionAndScaleTo(res);
    }

    // XXX support viewconfig when needed
}
 
function setupDisplayport(contentRootElement) {
    if (!contentRootElement) {
        return;
    }

    function setupDisplayportForElement(element, winUtils) {
        var dpw = attrOrDefault(element, "reftest-displayport-w", 0);
        var dph = attrOrDefault(element, "reftest-displayport-h", 0);
        var dpx = attrOrDefault(element, "reftest-displayport-x", 0);
        var dpy = attrOrDefault(element, "reftest-displayport-y", 0);
        if (dpw !== 0 || dph !== 0 || dpx != 0 || dpy != 0) {
            LogInfo("Setting displayport to <x="+ dpx +", y="+ dpy +", w="+ dpw +", h="+ dph +">");
            winUtils.setDisplayPortForElement(dpx, dpy, dpw, dph, element, 1);
        }
    }

    function setupDisplayportForElementSubtree(element, winUtils) {
        setupDisplayportForElement(element, winUtils);
        for (var c = element.firstElementChild; c; c = c.nextElementSibling) {
            setupDisplayportForElementSubtree(c, winUtils);
        }
        if (element.contentDocument) {
            LogInfo("Descending into subdocument");
            setupDisplayportForElementSubtree(element.contentDocument.documentElement,
                                              windowUtilsForWindow(element.contentWindow));
        }
    }

    if (contentRootElement.hasAttribute("reftest-async-scroll")) {
        setupDisplayportForElementSubtree(contentRootElement, windowUtils());
    } else {
        setupDisplayportForElement(contentRootElement, windowUtils());
    }
}

// Returns whether any offsets were updated
function setupAsyncScrollOffsets(options) {
    var currentDoc = content.document;
    var contentRootElement = currentDoc ? currentDoc.documentElement : null;

    if (!contentRootElement) {
        return false;
    }

    function setupAsyncScrollOffsetsForElement(element, winUtils) {
        var sx = attrOrDefault(element, "reftest-async-scroll-x", 0);
        var sy = attrOrDefault(element, "reftest-async-scroll-y", 0);
        if (sx != 0 || sy != 0) {
            try {
                // This might fail when called from RecordResult since layers
                // may not have been constructed yet
                winUtils.setAsyncScrollOffset(element, sx, sy);
                return true;
            } catch (e) {
                if (!options.allowFailure) {
                    throw e;
                }
            }
        }
        return false;
    }

    function setupAsyncScrollOffsetsForElementSubtree(element, winUtils) {
        var updatedAny = setupAsyncScrollOffsetsForElement(element, winUtils);
        for (var c = element.firstElementChild; c; c = c.nextElementSibling) {
            if (setupAsyncScrollOffsetsForElementSubtree(c, winUtils)) {
                updatedAny = true;
            }
        }
        if (element.contentDocument) {
            LogInfo("Descending into subdocument (async offsets)");
            if (setupAsyncScrollOffsetsForElementSubtree(element.contentDocument.documentElement,
                                                         windowUtilsForWindow(element.contentWindow))) {
                updatedAny = true;
            }
        }
        return updatedAny;
    }

    var asyncScroll = contentRootElement.hasAttribute("reftest-async-scroll");
    if (asyncScroll) {
        return setupAsyncScrollOffsetsForElementSubtree(contentRootElement, windowUtils());
    }
    return false;
}

function setupAsyncZoom(options) {
    var currentDoc = content.document;
    var contentRootElement = currentDoc ? currentDoc.documentElement : null;

    if (!contentRootElement || !contentRootElement.hasAttribute('reftest-async-zoom'))
        return false;

    var zoom = attrOrDefault(contentRootElement, "reftest-async-zoom", 1);
    if (zoom != 1) {
        try {
            windowUtils().setAsyncZoom(contentRootElement, zoom);
            return true;
        } catch (e) {
            if (!options.allowFailure) {
                throw e;
            }
        }
    }
    return false;
}


function resetDisplayportAndViewport() {
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
                             .includes("reftest-wait");
}

function shouldSnapshotWholePage(contentRootElement) {
    // use getAttribute because className works differently in HTML and SVG
    return contentRootElement &&
           contentRootElement.hasAttribute('class') &&
           contentRootElement.getAttribute('class').split(/\s+/)
                             .includes("reftest-snapshot-all");
}

function getNoPaintElements(contentRootElement) {
    return contentRootElement.getElementsByClassName('reftest-no-paint');
}
function getNoDisplayListElements(contentRootElement) {
    return contentRootElement.getElementsByClassName('reftest-no-display-list');
}
function getDisplayListElements(contentRootElement) {
    return contentRootElement.getElementsByClassName('reftest-display-list');
}

function getOpaqueLayerElements(contentRootElement) {
    return contentRootElement.getElementsByClassName('reftest-opaque-layer');
}

function getAssignedLayerMap(contentRootElement) {
    var layerNameToElementsMap = {};
    var elements = contentRootElement.querySelectorAll('[reftest-assigned-layer]');
    for (var i = 0; i < elements.length; ++i) {
        var element = elements[i];
        var layerName = element.getAttribute('reftest-assigned-layer');
        if (!(layerName in layerNameToElementsMap)) {
            layerNameToElementsMap[layerName] = [];
        }
        layerNameToElementsMap[layerName].push(element);
    }
    return layerNameToElementsMap;
}

const FlushMode = {
  ALL: 0,
  IGNORE_THROTTLED_ANIMATIONS: 1
};

// Initial state. When the document has loaded and all MozAfterPaint events and
// all explicit paint waits are flushed, we can fire the MozReftestInvalidate
// event and move to the next state.
const STATE_WAITING_TO_FIRE_INVALIDATE_EVENT = 0;
// When reftest-wait has been removed from the root element, we can move to the
// next state.
const STATE_WAITING_FOR_REFTEST_WAIT_REMOVAL = 1;
// When spell checking is done on all spell-checked elements, we can move to the
// next state.
const STATE_WAITING_FOR_SPELL_CHECKS = 2;
// When any pending compositor-side repaint requests have been flushed, we can
// move to the next state.
const STATE_WAITING_FOR_APZ_FLUSH = 3;
// When all MozAfterPaint events and all explicit paint waits are flushed, we're
// done and can move to the COMPLETED state.
const STATE_WAITING_TO_FINISH = 4;
const STATE_COMPLETED = 5;

function FlushRendering(aFlushMode) {
    let browsingContext = content.docShell.browsingContext;
    let ignoreThrottledAnimations = (aFlushMode === FlushMode.IGNORE_THROTTLED_ANIMATIONS);
    let promise = content.windowGlobalChild.getActor("ReftestFission").sendQuery("FlushRendering", {browsingContext, ignoreThrottledAnimations});
    return promise.then(function(result) {
        for (let errorString of result.errorStrings) {
            LogError(errorString);
        }
        for (let warningString of result.warningStrings) {
            LogWarning(warningString);
        }
        for (let infoString of result.infoStrings) {
            LogInfo(infoString);
        }
    }, function(reason) {
        // We expect actors to go away causing sendQuery's to fail, so
        // just note it.
        LogInfo("FlushRendering sendQuery to parent rejected: " + reason);
    });
}

function WaitForTestEnd(contentRootElement, inPrintMode, spellCheckedElements, forURL) {
    // WaitForTestEnd works via the MakeProgress function below. It is responsible for
    // moving through the states listed above and calling FlushRendering. We also listen
    // for a number of events, the most important of which is the AfterPaintListener,
    // which is responsible for updating the canvas after paints. In a fission world
    // FlushRendering and updating the canvas must necessarily be async operations.
    // During these async operations we want to wait for them to finish and we don't
    // want to try to do anything else (what would we even want to do while only some of
    // the processes involved have flushed layout or updated their layer trees?). So
    // we call OperationInProgress whenever we are about to go back to the event loop
    // during one of these calls, and OperationCompleted when it finishes. This prevents
    // anything else from running while we wait and getting us into a confused state. We
    // then record anything that happens while we are waiting to make sure that the
    // right actions are triggered. The possible actions are basically calling
    // MakeProgress from a setTimeout, and updating the canvas for an after paint event.
    // The after paint listener just stashes the rects and we update them after a
    // completed MakeProgress call. This is handled by
    // HandlePendingTasksAfterMakeProgress, which also waits for any pending after paint
    // events. The general sequence of events is:
    //   - MakeProgress
    //   - HandlePendingTasksAfterMakeProgress
    //     - wait for after paint event if one is pending
    //     - update canvas for after paint events we have received
    //   - MakeProgress
    //   etc

    function CheckForLivenessOfContentRootElement() {
        if (contentRootElement && Cu.isDeadWrapper(contentRootElement)) {
            contentRootElement = null;
        }
    }

    var setTimeoutCallMakeProgressWhenComplete = false;

    var operationInProgress = false;
    function OperationInProgress() {
        if (operationInProgress != false) {
            LogWarning("Nesting atomic operations?");
        }
        operationInProgress = true;
    }
    function OperationCompleted() {
        if (operationInProgress != true) {
            LogWarning("Mismatched OperationInProgress/OperationCompleted calls?");
        }
        operationInProgress = false;
        if (setTimeoutCallMakeProgressWhenComplete) {
            setTimeoutCallMakeProgressWhenComplete = false;
            setTimeout(CallMakeProgress, 0);
        }
    }
    function AssertNoOperationInProgress() {
        if (operationInProgress) {
            LogWarning("AssertNoOperationInProgress but operationInProgress");
        }
    }

    var updateCanvasPending = false;
    var updateCanvasRects = [];

    var stopAfterPaintReceived = false;
    var currentDoc = content.document;
    var state = STATE_WAITING_TO_FIRE_INVALIDATE_EVENT;

    var setTimeoutMakeProgressPending = false;

    function CallSetTimeoutMakeProgress() {
        if (setTimeoutMakeProgressPending) {
            return;
        }
        setTimeoutMakeProgressPending = true;
        setTimeout(CallMakeProgress, 0);
    }

    // This should only ever be called from a timeout.
    function CallMakeProgress() {
        if (operationInProgress) {
            setTimeoutCallMakeProgressWhenComplete = true;
            return;
        }
        setTimeoutMakeProgressPending = false;
        MakeProgress();
    }

    var waitingForAnAfterPaint = false;

    // Updates the canvas if there are pending updates for it. Checks if we
    // need to call MakeProgress.
    function HandlePendingTasksAfterMakeProgress() {
        AssertNoOperationInProgress();

        if ((state == STATE_WAITING_TO_FIRE_INVALIDATE_EVENT || state == STATE_WAITING_TO_FINISH) &&
            shouldWaitForPendingPaints()) {
            LogInfo("HandlePendingTasksAfterMakeProgress waiting for a MozAfterPaint");
            // We are in a state where we wait for MozAfterPaint to clear and a
            // MozAfterPaint event is pending, give it a chance to fire, but don't
            // let anything else run.
            waitingForAnAfterPaint = true;
            OperationInProgress();
            return;
        }

        if (updateCanvasPending) {
            LogInfo("HandlePendingTasksAfterMakeProgress updating canvas");
            updateCanvasPending = false;
            let rects = updateCanvasRects;
            updateCanvasRects = [];
            OperationInProgress();
            CheckForLivenessOfContentRootElement();
            let promise = SendUpdateCanvasForEvent(forURL, rects, contentRootElement);
            promise.then(function () {
                OperationCompleted();
                // After paint events are fired immediately after a paint (one
                // of the things that can call us). Don't confuse ourselves by
                // firing synchronously if we triggered the paint ourselves.
                CallSetTimeoutMakeProgress();
            });
        }
    }

    // true if rectA contains rectB
    function Contains(rectA, rectB) {
        return (rectA.left <= rectB.left && rectB.right <= rectA.right && rectA.top <= rectB.top && rectB.bottom <= rectA.bottom);
    }
    // true if some rect in rectList contains rect
    function ContainedIn(rectList, rect) {
        for (let i = 0; i < rectList.length; ++i) {
            if (Contains(rectList[i], rect)) {
                return true;
            }
        }
        return false;
    }

    function AfterPaintListener(event) {
        LogInfo("AfterPaintListener in " + event.target.document.location.href);
        if (event.target.document != currentDoc) {
            // ignore paint events for subframes or old documents in the window.
            // Invalidation in subframes will cause invalidation in the toplevel document anyway.
            return;
        }

        updateCanvasPending = true;
        for (let r of event.clientRects) {
            if (ContainedIn(updateCanvasRects, r)) {
                continue;
            }

            // Copy the rect; it's content and we are chrome, which means if the
            // document goes away (and it can in some crashtests) our reference
            // to it will be turned into a dead wrapper that we can't acccess.
            updateCanvasRects.push({ left: r.left, top: r.top, right: r.right, bottom: r.bottom });
        }

        if (waitingForAnAfterPaint) {
            waitingForAnAfterPaint = false;
            OperationCompleted();
        }

        if (!operationInProgress) {
            HandlePendingTasksAfterMakeProgress();
        }
        // Otherwise we know that eventually after the operation finishes we
        // will get a MakeProgress and/or HandlePendingTasksAfterMakeProgress
        // call, so we don't need to do anything.
    }

    function FromChildAfterPaintListener(event) {
        LogInfo("FromChildAfterPaintListener from " + event.detail.originalTargetUri);

        updateCanvasPending = true;
        for (let r of event.detail.rects) {
            if (ContainedIn(updateCanvasRects, r)) {
                continue;
            }

            // Copy the rect; it's content and we are chrome, which means if the
            // document goes away (and it can in some crashtests) our reference
            // to it will be turned into a dead wrapper that we can't acccess.
            updateCanvasRects.push({ left: r.left, top: r.top, right: r.right, bottom: r.bottom });
        }

        if (!operationInProgress) {
            HandlePendingTasksAfterMakeProgress();
        }
        // Otherwise we know that eventually after the operation finishes we
        // will get a MakeProgress and/or HandlePendingTasksAfterMakeProgress
        // call, so we don't need to do anything.
    }

    function AttrModifiedListener() {
        LogInfo("AttrModifiedListener fired");
        // Wait for the next return-to-event-loop before continuing --- for
        // example, the attribute may have been modified in an subdocument's
        // load event handler, in which case we need load event processing
        // to complete and unsuppress painting before we check isMozAfterPaintPending.
        CallSetTimeoutMakeProgress();
    }

    function ExplicitPaintsCompleteListener() {
        LogInfo("ExplicitPaintsCompleteListener fired");
        // Since this can fire while painting, don't confuse ourselves by
        // firing synchronously. It's fine to do this asynchronously.
        CallSetTimeoutMakeProgress();
    }

    function RemoveListeners() {
        // OK, we can end the test now.
        removeEventListener("MozAfterPaint", AfterPaintListener, false);
        removeEventListener("Reftest:MozAfterPaintFromChild", FromChildAfterPaintListener, false);
        CheckForLivenessOfContentRootElement();
        if (contentRootElement) {
            contentRootElement.removeEventListener("DOMAttrModified", AttrModifiedListener);
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

        LogInfo("MakeProgress");

        // We don't need to flush styles any more when we are in the state
        // after reftest-wait has removed.
        OperationInProgress();
        let promise = Promise.resolve(undefined);
        if (state != STATE_WAITING_TO_FINISH) {
          // If we are waiting for the MozReftestInvalidate event we don't want
          // to flush throttled animations. Flushing throttled animations can
          // continue to cause new MozAfterPaint events even when all the
          // rendering we're concerned about should have ceased. Since
          // MozReftestInvalidate won't be sent until we finish waiting for all
          // MozAfterPaint events, we should avoid flushing throttled animations
          // here or else we'll never leave this state.
          flushMode = (state === STATE_WAITING_TO_FIRE_INVALIDATE_EVENT)
                    ? FlushMode.IGNORE_THROTTLED_ANIMATIONS
                    : FlushMode.ALL;
          promise = FlushRendering(flushMode);
        }
        promise.then(function () {
            OperationCompleted();
            MakeProgress2();
            // If there is an operation in progress then we know there will be
            // a MakeProgress call is will happen after it finishes.
            if (!operationInProgress) {
                HandlePendingTasksAfterMakeProgress();
            }
        });
    }

    function MakeProgress2() {
        switch (state) {
        case STATE_WAITING_TO_FIRE_INVALIDATE_EVENT: {
            LogInfo("MakeProgress: STATE_WAITING_TO_FIRE_INVALIDATE_EVENT");
            if (shouldWaitForExplicitPaintWaiters() || shouldWaitForPendingPaints() ||
                updateCanvasPending) {
                gFailureReason = "timed out waiting for pending paint count to reach zero";
                if (shouldWaitForExplicitPaintWaiters()) {
                    gFailureReason += " (waiting for MozPaintWaitFinished)";
                    LogInfo("MakeProgress: waiting for MozPaintWaitFinished");
                }
                if (shouldWaitForPendingPaints()) {
                    gFailureReason += " (waiting for MozAfterPaint)";
                    LogInfo("MakeProgress: waiting for MozAfterPaint");
                }
                if (updateCanvasPending) {
                    gFailureReason += " (waiting for updateCanvasPending)";
                    LogInfo("MakeProgress: waiting for updateCanvasPending");
                }
                return;
            }

            state = STATE_WAITING_FOR_REFTEST_WAIT_REMOVAL;
            CheckForLivenessOfContentRootElement();
            var hasReftestWait = shouldWaitForReftestWaitRemoval(contentRootElement);
            // Notify the test document that now is a good time to test some invalidation
            LogInfo("MakeProgress: dispatching MozReftestInvalidate");
            if (contentRootElement) {
                var elements = getNoPaintElements(contentRootElement);
                for (var i = 0; i < elements.length; ++i) {
                  windowUtils().checkAndClearPaintedState(elements[i]);
                }
                elements = getNoDisplayListElements(contentRootElement);
                for (var i = 0; i < elements.length; ++i) {
                  windowUtils().checkAndClearDisplayListState(elements[i]);
                }
                elements = getDisplayListElements(contentRootElement);
                for (var i = 0; i < elements.length; ++i) {
                  windowUtils().checkAndClearDisplayListState(elements[i]);
                }
                var notification = content.document.createEvent("Events");
                notification.initEvent("MozReftestInvalidate", true, false);
                contentRootElement.dispatchEvent(notification);
            }

            if (!inPrintMode && doPrintMode(contentRootElement)) {
                LogInfo("MakeProgress: setting up print mode");
                setupPrintMode();
            }

            if (hasReftestWait && !shouldWaitForReftestWaitRemoval(contentRootElement)) {
                // MozReftestInvalidate handler removed reftest-wait.
                // We expect something to have been invalidated...
                OperationInProgress();
                let promise = FlushRendering(FlushMode.ALL);
                promise.then(function () {
                    OperationCompleted();
                    if (!updateCanvasPending && !shouldWaitForPendingPaints() &&
                        !shouldWaitForExplicitPaintWaiters()) {
                        LogWarning("MozInvalidateEvent didn't invalidate");
                    }
                    MakeProgress();
                });
                return;
            }
            // Try next state
            MakeProgress();
            return;
        }

        case STATE_WAITING_FOR_REFTEST_WAIT_REMOVAL:
            LogInfo("MakeProgress: STATE_WAITING_FOR_REFTEST_WAIT_REMOVAL");
            CheckForLivenessOfContentRootElement();
            if (shouldWaitForReftestWaitRemoval(contentRootElement)) {
                gFailureReason = "timed out waiting for reftest-wait to be removed";
                LogInfo("MakeProgress: waiting for reftest-wait to be removed");
                return;
            }

            // Try next state
            state = STATE_WAITING_FOR_SPELL_CHECKS;
            MakeProgress();
            return;

        case STATE_WAITING_FOR_SPELL_CHECKS:
            LogInfo("MakeProgress: STATE_WAITING_FOR_SPELL_CHECKS");
            if (numPendingSpellChecks) {
                gFailureReason = "timed out waiting for spell checks to end";
                LogInfo("MakeProgress: waiting for spell checks to end");
                return;
            }

            state = STATE_WAITING_FOR_APZ_FLUSH;
            LogInfo("MakeProgress: STATE_WAITING_FOR_APZ_FLUSH");
            gFailureReason = "timed out waiting for APZ flush to complete";

            var os = Cc[NS_OBSERVER_SERVICE_CONTRACTID].getService(Ci.nsIObserverService);
            var flushWaiter = function(aSubject, aTopic, aData) {
                if (aTopic) LogInfo("MakeProgress: apz-repaints-flushed fired");
                os.removeObserver(flushWaiter, "apz-repaints-flushed");
                state = STATE_WAITING_TO_FINISH;
                if (operationInProgress) {
                    CallSetTimeoutMakeProgress();
                } else {
                    MakeProgress();
                }
            };
            os.addObserver(flushWaiter, "apz-repaints-flushed");

            var willSnapshot = IsSnapshottableTestType();
            CheckForLivenessOfContentRootElement();
            var noFlush =
                !(contentRootElement &&
                  contentRootElement.classList.contains("reftest-no-flush"));
            if (noFlush && willSnapshot && windowUtils().flushApzRepaints()) {
                LogInfo("MakeProgress: done requesting APZ flush");
            } else {
                LogInfo("MakeProgress: APZ flush not required");
                flushWaiter(null, null, null);
            }
            return;

        case STATE_WAITING_FOR_APZ_FLUSH:
            LogInfo("MakeProgress: STATE_WAITING_FOR_APZ_FLUSH");
            // Nothing to do here; once we get the apz-repaints-flushed event
            // we will go to STATE_WAITING_TO_FINISH
            return;

        case STATE_WAITING_TO_FINISH:
            LogInfo("MakeProgress: STATE_WAITING_TO_FINISH");
            if (shouldWaitForExplicitPaintWaiters() || shouldWaitForPendingPaints() ||
                updateCanvasPending) {
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
                if (updateCanvasPending) {
                    gFailureReason += " (waiting for updateCanvasPending)";
                    LogInfo("MakeProgress: waiting for updateCanvasPending");
                }
                return;
            }
            CheckForLivenessOfContentRootElement();
            if (contentRootElement) {
              var elements = getNoPaintElements(contentRootElement);
              for (var i = 0; i < elements.length; ++i) {
                  if (windowUtils().checkAndClearPaintedState(elements[i])) {
                      SendFailedNoPaint();
                  }
              }
              // We only support retained display lists in the content process
              // right now, so don't fail reftest-no-display-list tests when
              // we don't have e10s.
              if (gBrowserIsRemote) {
                elements = getNoDisplayListElements(contentRootElement);
                for (var i = 0; i < elements.length; ++i) {
                    if (windowUtils().checkAndClearDisplayListState(elements[i])) {
                        SendFailedNoDisplayList();
                    }
                }
                elements = getDisplayListElements(contentRootElement);
                for (var i = 0; i < elements.length; ++i) {
                    if (!windowUtils().checkAndClearDisplayListState(elements[i])) {
                        SendFailedDisplayList();
                    }
                }
              }
              CheckLayerAssertions(contentRootElement);
            }

            if (!IsSnapshottableTestType()) {
              // If we're not snapshotting the test, at least do a sync round-trip
              // to the compositor to ensure that all the rendering messages
              // related to this test get processed. Otherwise problems triggered
              // by this test may only manifest as failures in a later test.
              LogInfo("MakeProgress: Doing sync flush to compositor");
              gFailureReason = "timed out while waiting for sync compositor flush"
              windowUtils().syncFlushCompositor();
            }

            LogInfo("MakeProgress: Completed");
            state = STATE_COMPLETED;
            gFailureReason = "timed out while taking snapshot (bug in harness?)";
            RemoveListeners();
            CheckForLivenessOfContentRootElement();
            CheckForProcessCrashExpectation(contentRootElement);
            setTimeout(RecordResult, 0, forURL);
            return;
        }
    }

    LogInfo("WaitForTestEnd: Adding listeners");
    addEventListener("MozAfterPaint", AfterPaintListener, false);
    addEventListener("Reftest:MozAfterPaintFromChild", FromChildAfterPaintListener, false);

    // If contentRootElement is null then shouldWaitForReftestWaitRemoval will
    // always return false so we don't need a listener anyway
    CheckForLivenessOfContentRootElement();
    if (contentRootElement) {
      contentRootElement.addEventListener("DOMAttrModified", AttrModifiedListener);
    }
    gExplicitPendingPaintsCompleteHook = ExplicitPaintsCompleteListener;
    gTimeoutHook = RemoveListeners;

    // Listen for spell checks on spell-checked elements.
    var numPendingSpellChecks = spellCheckedElements.length;
    function decNumPendingSpellChecks() {
        --numPendingSpellChecks;
        if (operationInProgress) {
            CallSetTimeoutMakeProgress();
        } else {
            MakeProgress();
        }
    }
    for (let editable of spellCheckedElements) {
        try {
            onSpellCheck(editable, decNumPendingSpellChecks);
        } catch (err) {
            // The element may not have an editor, so ignore it.
            setTimeout(decNumPendingSpellChecks, 0);
        }
    }

    // Take a full snapshot now that all our listeners are set up. This
    // ensures it's impossible for us to miss updates between taking the snapshot
    // and adding our listeners.
    OperationInProgress();
    let promise = SendInitCanvasWithSnapshot(forURL);
    promise.then(function () {
        OperationCompleted();
        MakeProgress();
    });
}

function OnDocumentLoad(event)
{
    var currentDoc = content.document;
    if (event.target != currentDoc)
        // Ignore load events for subframes.
        return;

    if (gClearingForAssertionCheck) {
        if (currentDoc.location.href == BLANK_URL_FOR_CLEARING) {
            DoAssertionCheck();
            return;
        }

        // It's likely the previous test document reloads itself and causes the
        // attempt of loading blank page fails. In this case we should retry
        // loading the blank page.
        LogInfo("Retry loading a blank page");
        LoadURI(BLANK_URL_FOR_CLEARING);
        return;
    }

    if (currentDoc.location.href != gCurrentURL) {
        LogInfo("OnDocumentLoad fired for previous document");
        // Ignore load events for previous documents.
        return;
    }

    let ourURL = currentDoc.location.href;

    // Collect all editable, spell-checked elements.  It may be the case that
    // not all the elements that match this selector will be spell checked: for
    // example, a textarea without a spellcheck attribute may have a parent with
    // spellcheck=false, or script may set spellcheck=false on an element whose
    // markup sets it to true.  But that's OK since onSpellCheck detects the
    // absence of spell checking, too.
    var querySelector =
        '*[class~="spell-checked"],' +
        'textarea:not([spellcheck="false"]),' +
        'input[spellcheck]:-moz-any([spellcheck=""],[spellcheck="true"]),' +
        '*[contenteditable]:-moz-any([contenteditable=""],[contenteditable="true"])';
    var spellCheckedElements = currentDoc.querySelectorAll(querySelector);

    var contentRootElement = currentDoc ? currentDoc.documentElement : null;
    currentDoc = null;
    setupFullZoom(contentRootElement);
    setupTextZoom(contentRootElement);
    setupViewport(contentRootElement);
    setupDisplayport(contentRootElement);
    var inPrintMode = false;

    async function AfterOnLoadScripts() {
        // Regrab the root element, because the document may have changed.
        var contentRootElement =
          content.document ? content.document.documentElement : null;

        // "MozPaintWait" events are dispatched using a scriptrunner, so we
        // receive then after painting has finished but before the main thread
        // returns from the paint call. Then a "MozPaintWaitFinished" is
        // dispatched to the main thread event loop.
        // Before Fission both the FlushRendering and SendInitCanvasWithSnapshot
        // calls were sync, but with Fission they must be async. So before Fission
        // we got the MozPaintWait event but not the MozPaintWaitFinished event
        // here (yet), which made us enter WaitForTestEnd. After Fission we get
        // both MozPaintWait and MozPaintWaitFinished here. So to make this work
        // the same way as before we just track if we got either event and go
        // into reftest-wait mode.
        var paintWaiterFinished = false;

        gExplicitPendingPaintsCompleteHook = function () {
            LogInfo("PaintWaiters finished while we were sending initial snapshop in AfterOnLoadScripts");
            paintWaiterFinished = true;
        }

        // Flush the document in case it got modified in a load event handler.
        await FlushRendering(FlushMode.ALL);

        // Take a snapshot now. We need to do this before we check whether
        // we should wait, since this might trigger dispatching of
        // MozPaintWait events and make shouldWaitForExplicitPaintWaiters() true
        // below.
        let painted = await SendInitCanvasWithSnapshot(ourURL);

        gExplicitPendingPaintsCompleteHook = null;

        if (contentRootElement && Cu.isDeadWrapper(contentRootElement)) {
            contentRootElement = null;
        }

        if (paintWaiterFinished || shouldWaitForExplicitPaintWaiters() ||
            (!inPrintMode && doPrintMode(contentRootElement)) ||
            // If we didn't force a paint above, in
            // InitCurrentCanvasWithSnapshot, so we should wait for a
            // paint before we consider them done.
            !painted) {
            LogInfo("AfterOnLoadScripts belatedly entering WaitForTestEnd");
            // Go into reftest-wait mode belatedly.
            WaitForTestEnd(contentRootElement, inPrintMode, [], ourURL);
        } else {
            CheckLayerAssertions(contentRootElement);
            CheckForProcessCrashExpectation(contentRootElement);
            RecordResult(ourURL);
        }
    }

    if (shouldWaitForReftestWaitRemoval(contentRootElement) ||
        shouldWaitForExplicitPaintWaiters() ||
        spellCheckedElements.length) {
        // Go into reftest-wait mode immediately after painting has been
        // unsuppressed, after the onload event has finished dispatching.
        gFailureReason = "timed out waiting for test to complete (trying to get into WaitForTestEnd)";
        LogInfo("OnDocumentLoad triggering WaitForTestEnd");
        setTimeout(function () { WaitForTestEnd(contentRootElement, inPrintMode, spellCheckedElements, ourURL); }, 0);
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

function CheckLayerAssertions(contentRootElement)
{
    if (!contentRootElement) {
        return;
    }
    if (gIsWebRenderEnabled) {
        // WebRender doesn't use layers, so let's not try checking layers
        // assertions.
        return;
    }

    var opaqueLayerElements = getOpaqueLayerElements(contentRootElement);
    for (var i = 0; i < opaqueLayerElements.length; ++i) {
        var elem = opaqueLayerElements[i];
        try {
            if (!windowUtils().isPartOfOpaqueLayer(elem)) {
                SendFailedOpaqueLayer(elementDescription(elem) + ' is not part of an opaque layer');
            }
        } catch (e) {
            SendFailedOpaqueLayer('got an exception while checking whether ' + elementDescription(elem) + ' is part of an opaque layer');
        }
    }
    var layerNameToElementsMap = getAssignedLayerMap(contentRootElement);
    var oneOfEach = [];
    // Check that elements with the same reftest-assigned-layer share the same PaintedLayer.
    for (var layerName in layerNameToElementsMap) {
        try {
            var elements = layerNameToElementsMap[layerName];
            oneOfEach.push(elements[0]);
            var numberOfLayers = windowUtils().numberOfAssignedPaintedLayers(elements);
            if (numberOfLayers !== 1) {
                SendFailedAssignedLayer('these elements are assigned to ' + numberOfLayers +
                                        ' different layers, instead of sharing just one layer: ' +
                                        elements.map(elementDescription).join(', '));
            }
        } catch (e) {
            SendFailedAssignedLayer('got an exception while checking whether these elements share a layer: ' +
                                    elements.map(elementDescription).join(', '));
        }
    }
    // Check that elements with different reftest-assigned-layer are assigned to different PaintedLayers.
    if (oneOfEach.length > 0) {
        try {
            var numberOfLayers = windowUtils().numberOfAssignedPaintedLayers(oneOfEach);
            if (numberOfLayers !== oneOfEach.length) {
                SendFailedAssignedLayer('these elements are assigned to ' + numberOfLayers +
                                        ' different layers, instead of having none in common (expected ' +
                                        oneOfEach.length + ' different layers): ' +
                                        oneOfEach.map(elementDescription).join(', '));
            }
        } catch (e) {
            SendFailedAssignedLayer('got an exception while checking whether these elements are assigned to different layers: ' +
                                    oneOfEach.map(elementDescription).join(', '));
        }
    }
}

function CheckForProcessCrashExpectation(contentRootElement)
{
    if (contentRootElement &&
        contentRootElement.hasAttribute('class') &&
        contentRootElement.getAttribute('class').split(/\s+/)
                          .includes("reftest-expect-process-crash")) {
        SendExpectProcessCrash();
    }
}

function RecordResult(forURL)
{
    if (forURL != gCurrentURL) {
        LogInfo("RecordResult fired for previous document");
        return;
    }

    if (gCurrentURLRecordResults > 0) {
        LogInfo("RecordResult fired extra times");
        FinishTestItem();
        return;
    }
    gCurrentURLRecordResults++;

    LogInfo("RecordResult fired");

    var currentTestRunTime = Date.now() - gCurrentTestStartTime;

    clearTimeout(gFailureTimeout);
    gFailureReason = null;
    gFailureTimeout = null;
    gCurrentURL = null;
    gCurrentURLTargetType = undefined;

    if (gCurrentTestType == TYPE_PRINT) {
        printToPdf(function (status, fileName) {
            SendPrintResult(currentTestRunTime, status, fileName);
            FinishTestItem();
        });
        return;
    }
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

    // Setup async scroll offsets now in case SynchronizeForSnapshot is not
    // called (due to reftest-no-sync-layers being supplied, or in the single
    // process case).
    var changedAsyncScrollZoom = false;
    if (setupAsyncScrollOffsets({allowFailure:true})) {
        changedAsyncScrollZoom = true;
    }
    if (setupAsyncZoom({allowFailure:true})) {
        changedAsyncScrollZoom = true;
    }
    if (changedAsyncScrollZoom && !gBrowserIsRemote) {
        sendAsyncMessage("reftest:UpdateWholeCanvasForInvalidation");
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
    let loadURIOptions = {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    };
    webNavigation().loadURI(uri, loadURIOptions);
}

function LogError(str)
{
    if (gVerbose) {
        sendSyncMessage("reftest:Log", { type: "error", msg: str });
    } else {
        sendAsyncMessage("reftest:Log", { type: "error", msg: str });
    }
}

function LogWarning(str)
{
    if (gVerbose) {
        sendSyncMessage("reftest:Log", { type: "warning", msg: str });
    } else {
        sendAsyncMessage("reftest:Log", { type: "warning", msg: str });
    }
}

function LogInfo(str)
{
    if (gVerbose) {
        sendSyncMessage("reftest:Log", { type: "info", msg: str });
    } else {
        sendAsyncMessage("reftest:Log", { type: "info", msg: str });
    }
}

function IsSnapshottableTestType()
{
    // Script, load-only, and PDF-print tests do not need any snapshotting.
    return !(gCurrentTestType == TYPE_SCRIPT ||
             gCurrentTestType == TYPE_LOAD ||
             gCurrentTestType == TYPE_PRINT);
}

const SYNC_DEFAULT = 0x0;
const SYNC_ALLOW_DISABLE = 0x1;
// Returns a promise that resolve when the snapshot is done.
function SynchronizeForSnapshot(flags)
{
    if (!IsSnapshottableTestType()) {
        return Promise.resolve(undefined);
    }

    if (flags & SYNC_ALLOW_DISABLE) {
        var docElt = content.document.documentElement;
        if (docElt &&
            (docElt.hasAttribute("reftest-no-sync-layers") ||
             docElt.classList.contains("reftest-no-flush"))) {
            LogInfo("Test file chose to skip SynchronizeForSnapshot");
            return Promise.resolve(undefined);
        }
    }

    let browsingContext = content.docShell.browsingContext;
    let promise = content.windowGlobalChild.getActor("ReftestFission").sendQuery("UpdateLayerTree", {browsingContext});
    return promise.then(function (result) {
        for (let errorString of result.errorStrings) {
            LogError(errorString);
        }
        for (let infoString of result.infoStrings) {
            LogInfo(infoString);
        }

        // Setup async scroll offsets now, because any scrollable layers should
        // have had their AsyncPanZoomControllers created.
        setupAsyncScrollOffsets({allowFailure:false});
        setupAsyncZoom({allowFailure:false});
    }, function(reason) {
        // We expect actors to go away causing sendQuery's to fail, so
        // just note it.
        LogInfo("UpdateLayerTree sendQuery to parent rejected: " + reason);

        // Setup async scroll offsets now, because any scrollable layers should
        // have had their AsyncPanZoomControllers created.
        setupAsyncScrollOffsets({allowFailure:false});
        setupAsyncZoom({allowFailure:false});
    });
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
        "reftest:LoadPrintTest",
        function (m) { RecvLoadPrintTest(m.json.uri, m.json.timeout); }
    );
    addMessageListener(
        "reftest:LoadTest",
        function (m) { RecvLoadTest(m.json.type, m.json.uri,
                                    m.json.uriTargetType,
                                    m.json.timeout); }
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

function RecvLoadTest(type, uri, uriTargetType, timeout)
{
    StartTestURI(type, uri, uriTargetType, timeout);
}

function RecvLoadScriptTest(uri, timeout)
{
    StartTestURI(TYPE_SCRIPT, uri, URL_TARGET_TYPE_TEST, timeout);
}

function RecvLoadPrintTest(uri, timeout)
{
    StartTestURI(TYPE_PRINT, uri, URL_TARGET_TYPE_TEST, timeout);
}

function RecvResetRenderingState()
{
    resetZoomAndTextZoom();
    resetDisplayportAndViewport();
}

function SendAssertionCount(numAssertions)
{
    sendAsyncMessage("reftest:AssertionCount", { count: numAssertions });
}

function SendContentReady()
{
    let gfxInfo = (NS_GFXINFO_CONTRACTID in Cc) && Cc[NS_GFXINFO_CONTRACTID].getService(Ci.nsIGfxInfo);
    let info = gfxInfo.getInfo();

    // The webrender check has to be separate from the d2d checks
    // since the d2d checks will throw an exception on non-windows platforms.
    try {
        gIsWebRenderEnabled = gfxInfo.WebRenderEnabled;
    } catch (e) {
        gIsWebRenderEnabled = false;
    }

    try {
        info.D2DEnabled = gfxInfo.D2DEnabled;
        info.DWriteEnabled = gfxInfo.DWriteEnabled;
    } catch (e) {
        info.D2DEnabled = false;
        info.DWriteEnabled = false;
    }

    return sendSyncMessage("reftest:ContentReady", { 'gfx': info })[0];
}

function SendException(what)
{
    sendAsyncMessage("reftest:Exception", { what: what });
}

function SendFailedLoad(why)
{
    sendAsyncMessage("reftest:FailedLoad", { why: why });
}

function SendFailedNoPaint()
{
    sendAsyncMessage("reftest:FailedNoPaint");
}

function SendFailedNoDisplayList()
{
    sendAsyncMessage("reftest:FailedNoDisplayList");
}

function SendFailedDisplayList()
{
    sendAsyncMessage("reftest:FailedDisplayList");
}

function SendFailedOpaqueLayer(why)
{
    sendAsyncMessage("reftest:FailedOpaqueLayer", { why: why });
}

function SendFailedAssignedLayer(why)
{
    sendAsyncMessage("reftest:FailedAssignedLayer", { why: why });
}

// Returns a promise that resolves to a bool that indicates if a snapshot was taken.
function SendInitCanvasWithSnapshot(forURL)
{
    if (forURL != gCurrentURL) {
        LogInfo("SendInitCanvasWithSnapshot called for previous document");
        // Lie and say we painted because it doesn't matter, this is a test we
        // are already done with that is clearing out. Then AfterOnLoadScripts
        // should finish quicker if that is who is calling us.
        return Promise.resolve(true);
    }

    // If we're in the same process as the top-level XUL window, then
    // drawing that window will also update our layers, so no
    // synchronization is needed.
    //
    // NB: this is a test-harness optimization only, it must not
    // affect the validity of the tests.
    if (gBrowserIsRemote) {
        let promise = SynchronizeForSnapshot(SYNC_DEFAULT);
        return promise.then(function () {
            let ret = sendSyncMessage("reftest:InitCanvasWithSnapshot")[0];

            gHaveCanvasSnapshot = ret.painted;
            return ret.painted;
        });
    }

    // For in-process browser, we have to make a synchronous request
    // here to make the above optimization valid, so that MozWaitPaint
    // events dispatched (synchronously) during painting are received
    // before we check the paint-wait counter.  For out-of-process
    // browser though, it doesn't wrt correctness whether this request
    // is sync or async.
    let ret = sendSyncMessage("reftest:InitCanvasWithSnapshot")[0];

    gHaveCanvasSnapshot = ret.painted;
    return Promise.resolve(ret.painted);
}

function SendScriptResults(runtimeMs, error, results)
{
    sendAsyncMessage("reftest:ScriptResults",
                     { runtimeMs: runtimeMs, error: error, results: results });
}

function SendPrintResult(runtimeMs, status, fileName)
{
    sendAsyncMessage("reftest:PrintResult",
                     { runtimeMs: runtimeMs, status: status, fileName: fileName });
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

function elementDescription(element)
{
    return '<' + element.localName +
        [].slice.call(element.attributes).map((attr) =>
            ` ${attr.nodeName}="${attr.value}"`).join('') +
        '>';
}

function SendUpdateCanvasForEvent(forURL, rectList, contentRootElement)
{
    if (forURL != gCurrentURL) {
        LogInfo("SendUpdateCanvasForEvent called for previous document");
        // This is a test we are already done with that is clearing out.
        // Don't do anything.
        return Promise.resolve(undefined);
    }

    var win = content;
    var scale = docShell.browsingContext.fullZoom;

    var rects = [ ];
    if (shouldSnapshotWholePage(contentRootElement)) {
      // See comments in SendInitCanvasWithSnapshot() re: the split
      // logic here.
      if (!gBrowserIsRemote) {
          sendSyncMessage("reftest:UpdateWholeCanvasForInvalidation");
      } else {
          let promise = SynchronizeForSnapshot(SYNC_ALLOW_DISABLE);
          return promise.then(function () {
            sendAsyncMessage("reftest:UpdateWholeCanvasForInvalidation");
          });
      }
      return Promise.resolve(undefined);
    }

    var message;
    if (gIsWebRenderEnabled && !windowUtils().isMozAfterPaintPending) {
        // Webrender doesn't have invalidation, so we just invalidate the whole
        // screen once we don't have anymore paints pending. This will force
        // the snapshot.

        LogInfo("Webrender enabled, sending update whole canvas for invalidation");
        message = "reftest:UpdateWholeCanvasForInvalidation";
    } else {
        LogInfo("SendUpdateCanvasForEvent with " + rectList.length + " rects");
        for (var i = 0; i < rectList.length; ++i) {
            var r = rectList[i];
            // Set left/top/right/bottom to "device pixel" boundaries
            var left = Math.floor(roundTo(r.left * scale, 0.001));
            var top = Math.floor(roundTo(r.top * scale, 0.001));
            var right = Math.ceil(roundTo(r.right * scale, 0.001));
            var bottom = Math.ceil(roundTo(r.bottom * scale, 0.001));
            LogInfo("Rect: " + left + " " + top + " " + right + " " + bottom);

            rects.push({ left: left, top: top, right: right, bottom: bottom });
        }

        message = "reftest:UpdateCanvasForInvalidation";
    }

    // See comments in SendInitCanvasWithSnapshot() re: the split
    // logic here.
    if (!gBrowserIsRemote) {
        sendSyncMessage(message, { rects: rects });
    } else {
        let promise = SynchronizeForSnapshot(SYNC_ALLOW_DISABLE);
        return promise.then(function () {
            sendAsyncMessage(message, { rects: rects });
        });
    }

    return Promise.resolve(undefined);
}

if (content.document.readyState == "complete") {
  // load event has already fired for content, get started
  OnInitialLoad();
} else {
  addEventListener("load", OnInitialLoad, true);
}
