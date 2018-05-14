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
Cu.import("chrome://reftest/content/AsyncSpellCheckTestHelper.jsm");
Cu.import("resource://gre/modules/Services.jsm");

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
var gRunSlower = false;

const TYPE_LOAD = 'load';  // test without a reference (just test that it does
                           // not assert, crash, hang, or leak)
const TYPE_SCRIPT = 'script'; // test contains individual test results
const TYPE_PRINT = 'print'; // test and reference will be printed to PDF's and
                            // compared structurally

// keep this in sync with globals.jsm
const URL_TARGET_TYPE_TEST = 0;      // first url
const URL_TARGET_TYPE_REFERENCE = 1; // second url, if any

function markupDocumentViewer() {
    return docShell.contentViewer;
}

function webNavigation() {
    return docShell.QueryInterface(Ci.nsIWebNavigation);
}

function windowUtilsForWindow(w) {
    return w.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils);
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
    var env = Cc[ENVIRONMENT_CONTRACTID].getService(Ci.nsIEnvironment);
    gVerbose = !!env.get("MOZ_REFTEST_VERBOSE");

    RegisterMessageListeners();

    var initInfo = SendContentReady();
    gBrowserIsRemote = initInfo.remote;
    gRunSlower = initInfo.runSlower;

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

    gCurrentTestStartTime = Date.now();
    if (gFailureTimeout != null) {
        SendException("program error managing timeouts\n");
    }
    SetFailureTimeout(LoadFailed, timeout, uri);

    LoadURI(gCurrentURL);
}

function setupFullZoom(contentRootElement) {
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
   docShell.contentViewer.setPageMode(true, ps);
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
    ps.printFrameType = Ci.nsIPrintSettings.kFramesAsIs;
    ps.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;

    if (isPrintSelection) {
        ps.printRange = Ci.nsIPrintSettings.kRangeSelection;
    } else if (printRange) {
        ps.printRange = Ci.nsIPrintSettings.kRangeSpecifiedPageRange;
        let range = printRange.split('-');
        ps.startPageRange = +range[0] || 1;
        ps.endPageRange = +range[1] || 1;
    }

    let webBrowserPrint = content.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebBrowserPrint);
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
        LogInfo("Setting scrollport to <w=" + sw + ", h=" + sh + ">");
        windowUtils().setScrollPositionClampingScrollPortSize(sw, sh);
    }

    // XXX support resolution when needed

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

function shouldWaitForPendingPaints(contentRootElement) {
    // if gHaveCanvasSnapshot is false, we're not taking snapshots so
    // there is no need to wait for pending paints to be flushed.
    return gHaveCanvasSnapshot &&
           !shouldIgnorePendingMozAfterPaints(contentRootElement) &&
           windowUtils().isMozAfterPaintPending;
}

function shouldIgnorePendingMozAfterPaints(contentRootElement) {
    // use getAttribute because className works differently in HTML and SVG
    var ignore = contentRootElement &&
           contentRootElement.hasAttribute('class') &&
           contentRootElement.getAttribute('class').split(/\s+/)
                             .includes("reftest-ignore-pending-paints");
    // getAnimations is nightly-only, so check it exists before calling it
    if (ignore && contentRootElement.ownerDocument.getAnimations
               && contentRootElement.ownerDocument.getAnimations().length == 0) {
      LogError("reftest-ignore-pending-paints should only be used on documents with animations!");
    }
    return ignore;
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
    var anyPendingPaintsGeneratedInDescendants = false;

    function flushWindow(win) {
        var utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
        var afterPaintWasPending = utils.isMozAfterPaintPending;

        var root = win.document.documentElement;
        if (root && !root.classList.contains("reftest-no-flush")) {
            try {
                if (aFlushMode === FlushMode.IGNORE_THROTTLED_ANIMATIONS) {
                    utils.flushLayoutWithoutThrottledAnimations();
                } else {
                    root.getBoundingClientRect();
                }
            } catch (e) {
                LogWarning("flushWindow failed: " + e + "\n");
            }
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

function WaitForTestEnd(contentRootElement, inPrintMode, spellCheckedElements) {
    var stopAfterPaintReceived = false;
    var currentDoc = content.document;
    var state = STATE_WAITING_TO_FIRE_INVALIDATE_EVENT;

    function AfterPaintListener(event) {
        LogInfo("AfterPaintListener in " + event.target.document.location.href);
        if (event.target.document != currentDoc) {
            // ignore paint events for subframes or old documents in the window.
            // Invalidation in subframes will cause invalidation in the toplevel document anyway.
            return;
        }

        SendUpdateCanvasForEvent(event, contentRootElement);
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

        // We don't need to flush styles any more when we are in the state
        // after reftest-wait has removed.
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
          FlushRendering(flushMode);
        }

        switch (state) {
        case STATE_WAITING_TO_FIRE_INVALIDATE_EVENT: {
            LogInfo("MakeProgress: STATE_WAITING_TO_FIRE_INVALIDATE_EVENT");
            if (shouldWaitForExplicitPaintWaiters() || shouldWaitForPendingPaints(contentRootElement)) {
                gFailureReason = "timed out waiting for pending paint count to reach zero";
                if (shouldWaitForExplicitPaintWaiters()) {
                    gFailureReason += " (waiting for MozPaintWaitFinished)";
                    LogInfo("MakeProgress: waiting for MozPaintWaitFinished");
                }
                if (shouldWaitForPendingPaints(contentRootElement)) {
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
                FlushRendering(FlushMode.ALL);
                if (!shouldWaitForPendingPaints(contentRootElement) && !shouldWaitForExplicitPaintWaiters()) {
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
                MakeProgress();
            };
            os.addObserver(flushWaiter, "apz-repaints-flushed");

            var willSnapshot = (gCurrentTestType != TYPE_SCRIPT) &&
                               (gCurrentTestType != TYPE_LOAD) &&
                               (gCurrentTestType != TYPE_PRINT);
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
            if (shouldWaitForExplicitPaintWaiters() || shouldWaitForPendingPaints(contentRootElement)) {
                gFailureReason = "timed out waiting for pending paint count to " +
                    "reach zero (after reftest-wait removed and switch to print mode)";
                if (shouldWaitForExplicitPaintWaiters()) {
                    gFailureReason += " (waiting for MozPaintWaitFinished)";
                    LogInfo("MakeProgress: waiting for MozPaintWaitFinished");
                }
                if (shouldWaitForPendingPaints(contentRootElement)) {
                    gFailureReason += " (waiting for MozAfterPaint)";
                    LogInfo("MakeProgress: waiting for MozAfterPaint");
                }
                return;
            }
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
      contentRootElement.addEventListener("DOMAttrModified", AttrModifiedListener);
    }
    gExplicitPendingPaintsCompleteHook = ExplicitPaintsCompleteListener;
    gTimeoutHook = RemoveListeners;

    // Listen for spell checks on spell-checked elements.
    var numPendingSpellChecks = spellCheckedElements.length;
    function decNumPendingSpellChecks() {
        --numPendingSpellChecks;
        MakeProgress();
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
    SendInitCanvasWithSnapshot();
    MakeProgress();
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
    setupViewport(contentRootElement);
    setupDisplayport(contentRootElement);
    var inPrintMode = false;

    function AfterOnLoadScripts() {
        // Regrab the root element, because the document may have changed.
        var contentRootElement =
          content.document ? content.document.documentElement : null;

        // Flush the document in case it got modified in a load event handler.
        FlushRendering(FlushMode.ALL);

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
            WaitForTestEnd(contentRootElement, inPrintMode, []);
        } else {
            CheckLayerAssertions(contentRootElement);
            CheckForProcessCrashExpectation(contentRootElement);
            RecordResult();
        }
    }

    if (shouldWaitForReftestWaitRemoval(contentRootElement) ||
        shouldWaitForExplicitPaintWaiters() ||
        spellCheckedElements.length) {
        // Go into reftest-wait mode immediately after painting has been
        // unsuppressed, after the onload event has finished dispatching.
        gFailureReason = "timed out waiting for test to complete (trying to get into WaitForTestEnd)";
        LogInfo("OnDocumentLoad triggering WaitForTestEnd");
        setTimeout(function () { WaitForTestEnd(contentRootElement, inPrintMode, spellCheckedElements); }, 0);
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
            var numberOfLayers = windowUtils().numberOfAssignedPaintedLayers(elements, elements.length);
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
            var numberOfLayers = windowUtils().numberOfAssignedPaintedLayers(oneOfEach, oneOfEach.length);
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

function RecordResult()
{
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
    var flags = webNavigation().LOAD_FLAGS_NONE;
    webNavigation().loadURI(uri, flags, null, null, null);
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

const SYNC_DEFAULT = 0x0;
const SYNC_ALLOW_DISABLE = 0x1;
function SynchronizeForSnapshot(flags)
{
    if (gCurrentTestType == TYPE_SCRIPT ||
        gCurrentTestType == TYPE_LOAD ||
        gCurrentTestType == TYPE_PRINT) {
        // Script, load-only, and PDF-print tests do not need any snapshotting.
        return;
    }

    if (flags & SYNC_ALLOW_DISABLE) {
        var docElt = content.document.documentElement;
        if (docElt &&
            (docElt.hasAttribute("reftest-no-sync-layers") ||
             docElt.classList.contains("reftest-no-flush"))) {
            LogInfo("Test file chose to skip SynchronizeForSnapshot");
            return;
        }
    }

    windowUtils().updateLayerTree();

    // Setup async scroll offsets now, because any scrollable layers should
    // have had their AsyncPanZoomControllers created.
    setupAsyncScrollOffsets({allowFailure:false});
    setupAsyncZoom({allowFailure:false});
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
    if (gRunSlower) {
        setTimeout(function () { LoadURI(BLANK_URL_FOR_CLEARING) }, 250);
    } else {
        LoadURI(BLANK_URL_FOR_CLEARING);
    }
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
    resetZoom();
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

function SendUpdateCanvasForEvent(event, contentRootElement)
{
    var win = content;
    var scale = markupDocumentViewer().fullZoom;

    var rects = [ ];
    if (shouldSnapshotWholePage(contentRootElement)) {
      // See comments in SendInitCanvasWithSnapshot() re: the split
      // logic here.
      if (!gBrowserIsRemote) {
          sendSyncMessage("reftest:UpdateWholeCanvasForInvalidation");
      } else {
          SynchronizeForSnapshot(SYNC_ALLOW_DISABLE);
          sendAsyncMessage("reftest:UpdateWholeCanvasForInvalidation");
      }
      return;
    }

    var message;
    if (gIsWebRenderEnabled && !windowUtils().isMozAfterPaintPending) {
        // Webrender doesn't have invalidation, so we just invalidate the whole
        // screen once we don't have anymore paints pending. This will force
        // the snapshot.

        LogInfo("Webrender enabled, sending update whole canvas for invalidation");
        message = "reftest:UpdateWholeCanvasForInvalidation";
    } else {
        var rectList = event.clientRects;
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
        SynchronizeForSnapshot(SYNC_ALLOW_DISABLE);
        sendAsyncMessage(message, { rects: rects });
    }
}
if (content.document.readyState == "complete") {
  // load event has already fired for content, get started
  OnInitialLoad();
} else {
  addEventListener("load", OnInitialLoad, true);
}
