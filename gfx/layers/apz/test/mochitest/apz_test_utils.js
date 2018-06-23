// Utilities for writing APZ tests using the framework added in bug 961289

// ----------------------------------------------------------------------
// Functions that convert the APZ test data into a more usable form.
// Every place we have a WebIDL sequence whose elements are dictionaries
// with two elements, a key, and a value, we convert this into a JS
// object with a property for each key/value pair. (This is the structure
// we really want, but we can't express in directly in WebIDL.)
// ----------------------------------------------------------------------

function convertEntries(entries) {
  var result = {};
  for (var i = 0; i < entries.length; ++i) {
    result[entries[i].key] = entries[i].value;
  }
  return result;
}

function getPropertyAsRect(scrollFrames, scrollId, prop) {
  SimpleTest.ok(scrollId in scrollFrames,
                'expected scroll frame data for scroll id ' + scrollId);
  var scrollFrameData = scrollFrames[scrollId];
  SimpleTest.ok('displayport' in scrollFrameData,
                'expected a ' + prop + ' for scroll id ' + scrollId);
  var value = scrollFrameData[prop];
  var pieces = value.replace(/[()\s]+/g, '').split(',');
  SimpleTest.is(pieces.length, 4, "expected string of form (x,y,w,h)");
  return { x: parseInt(pieces[0]),
           y: parseInt(pieces[1]),
           w: parseInt(pieces[2]),
           h: parseInt(pieces[3]) };
}

function convertScrollFrameData(scrollFrames) {
  var result = {};
  for (var i = 0; i < scrollFrames.length; ++i) {
    result[scrollFrames[i].scrollId] = convertEntries(scrollFrames[i].entries);
  }
  return result;
}

function convertBuckets(buckets) {
  var result = {};
  for (var i = 0; i < buckets.length; ++i) {
    result[buckets[i].sequenceNumber] = convertScrollFrameData(buckets[i].scrollFrames);
  }
  return result;
}

function convertTestData(testData) {
  var result = {};
  result.paints = convertBuckets(testData.paints);
  result.repaintRequests = convertBuckets(testData.repaintRequests);
  return result;
}

// Given APZ test data for a single paint on the compositor side,
// reconstruct the APZC tree structure from the 'parentScrollId'
// entries that were logged. More specifically, the subset of the
// APZC tree structure corresponding to the layer subtree for the
// content process that triggered the paint, is reconstructed (as
// the APZ test data only contains information abot this subtree).
function buildApzcTree(paint) {
  // The APZC tree can potentially have multiple root nodes,
  // so we invent a node that is the parent of all roots.
  // This 'root' does not correspond to an APZC.
  var root = {scrollId: -1, children: []};
  for (var scrollId in paint) {
    paint[scrollId].children = [];
    paint[scrollId].scrollId = scrollId;
  }
  for (var scrollId in paint) {
    var parentNode = null;
    if ("hasNoParentWithSameLayersId" in paint[scrollId]) {
      parentNode = root;
    } else if ("parentScrollId" in paint[scrollId]) {
      parentNode = paint[paint[scrollId].parentScrollId];
    }
    parentNode.children.push(paint[scrollId]);
  }
  return root;
}

// Given an APZC tree produced by buildApzcTree, return the RCD node in
// the tree, or null if there was none.
function findRcdNode(apzcTree) {
  if (!!apzcTree.isRootContent) { // isRootContent will be undefined or "1"
    return apzcTree;
  }
  for (var i = 0; i < apzcTree.children.length; i++) {
    var rcd = findRcdNode(apzcTree.children[i]);
    if (rcd != null) {
      return rcd;
    }
  }
  return null;
}

// Return whether an element whose id includes |elementId| has been layerized.
// Assumes |elementId| will be present in the content description for the
// element, and not in the content descriptions of other elements.
function isLayerized(elementId) {
  var contentTestData = SpecialPowers.getDOMWindowUtils(window).getContentAPZTestData();
  ok(contentTestData.paints.length > 0, "expected at least one paint");
  var seqno = contentTestData.paints[contentTestData.paints.length - 1].sequenceNumber;
  contentTestData = convertTestData(contentTestData);
  var paint = contentTestData.paints[seqno];
  for (var scrollId in paint) {
    if ("contentDescription" in paint[scrollId]) {
      if (paint[scrollId]["contentDescription"].includes(elementId)) {
        return true;
      }
    }
  }
  return false;
}

function promiseApzRepaintsFlushed(aWindow = window) {
  return new Promise(function (resolve, reject) {
    var repaintDone = function() {
      SpecialPowers.Services.obs.removeObserver(repaintDone, "apz-repaints-flushed");
      setTimeout(resolve, 0);
    };
    SpecialPowers.Services.obs.addObserver(repaintDone, "apz-repaints-flushed");
    if (SpecialPowers.getDOMWindowUtils(aWindow).flushApzRepaints()) {
      dump("Flushed APZ repaints, waiting for callback...\n");
    } else {
      dump("Flushing APZ repaints was a no-op, triggering callback directly...\n");
      repaintDone();
    }
  });
}

function flushApzRepaints(aCallback, aWindow = window) {
  if (!aCallback) {
    throw "A callback must be provided!";
  }
  promiseApzRepaintsFlushed(aWindow).then(aCallback);
}

// Flush repaints, APZ pending repaints, and any repaints resulting from that
// flush. This is particularly useful if the test needs to reach some sort of
// "idle" state in terms of repaints. Usually just waiting for all paints
// followed by flushApzRepaints is sufficient to flush all APZ state back to
// the main thread, but it can leave a paint scheduled which will get triggered
// at some later time. For tests that specifically test for painting at
// specific times, this method is the way to go. Even if in doubt, this is the
// preferred method as the extra step is "safe" and shouldn't interfere with
// most tests.
function waitForApzFlushedRepaints(aCallback) {
  // First flush the main-thread paints and send transactions to the APZ
  promiseAllPaintsDone()
    // Then flush the APZ to make sure any repaint requests have been sent
    // back to the main thread. Note that we need a wrapper function around
    // promiseApzRepaintsFlushed otherwise the rect produced by
    // promiseAllPaintsDone gets passed to it as the window parameter.
    .then(() => promiseApzRepaintsFlushed())
    // Then flush the main-thread again to process the repaint requests.
    // Once this is done, we should be in a stable state with nothing
    // pending, so we can trigger the callback.
    .then(promiseAllPaintsDone)
    // Then allow the callback to be triggered.
    .then(aCallback);
}

// This function takes a set of subtests to run one at a time in new top-level
// windows, and returns a Promise that is resolved once all the subtests are
// done running.
//
// The aSubtests array is an array of objects with the following keys:
//   file: required, the filename of the subtest.
//   prefs: optional, an array of arrays containing key-value prefs to set.
//   dp_suppression: optional, a boolean on whether or not to respect displayport
//                   suppression during the test.
//   onload: optional, a function that will be registered as a load event listener
//           for the child window that will hold the subtest. the function will be
//           passed exactly one argument, which will be the child window.
// An example of an array is:
//   aSubtests = [
//     { 'file': 'test_file_name.html' },
//     { 'file': 'test_file_2.html', 'prefs': [['pref.name', true], ['other.pref', 1000]], 'dp_suppression': false }
//     { 'file': 'file_3.html', 'onload': function(w) { w.subtestDone(); } }
//   ];
//
// Each subtest should call the subtestDone() function when it is done, to
// indicate that the window should be torn down and the next text should run.
// The subtestDone() function is injected into the subtest's window by this
// function prior to loading the subtest. For convenience, the |is| and |ok|
// functions provided by SimpleTest are also mapped into the subtest's window.
// For other things from the parent, the subtest can use window.opener.<whatever>
// to access objects.
function runSubtestsSeriallyInFreshWindows(aSubtests) {
  return new Promise(function(resolve, reject) {
    var testIndex = -1;
    var w = null;

    // If the "apz.subtest" pref has been set, only a single subtest whose name matches
    // the pref's value (if any) will be run.
    var onlyOneSubtest = SpecialPowers.getCharPref("apz.subtest", /* default = */ "");

    function advanceSubtestExecution() {
      var test = aSubtests[testIndex];
      if (w) {
        // Run any cleanup functions registered in the subtest
        if (w.ApzCleanup) { // guard against the subtest not loading apz_test_utils.js
          w.ApzCleanup.execute();
        }
        if (typeof test.dp_suppression != 'undefined') {
          // We modified the suppression when starting the test, so now undo that.
          SpecialPowers.getDOMWindowUtils(window).respectDisplayPortSuppression(!test.dp_suppression);
        }
        if (test.prefs) {
          // We pushed some prefs for this test, pop them, and re-invoke
          // advanceSubtestExecution() after that's been processed
          SpecialPowers.popPrefEnv(function() {
            w.close();
            w = null;
            advanceSubtestExecution();
          });
          return;
        }

        w.close();
      }

      testIndex++;
      if (testIndex >= aSubtests.length) {
        resolve();
        return;
      }

      test = aSubtests[testIndex];

      if (onlyOneSubtest && onlyOneSubtest != test.file) {
        SimpleTest.ok(true, "Skipping " + test.file + " because only " + onlyOneSubtest + " is being run");
        setTimeout(function() { advanceSubtestExecution(); }, 0);
        return;
      }

      if (typeof test.dp_suppression != 'undefined') {
        // Normally during a test, the displayport will get suppressed during page
        // load, and unsuppressed at a non-deterministic time during the test. The
        // unsuppression can trigger a repaint which interferes with the test, so
        // to avoid that we can force the displayport to be unsuppressed for the
        // entire test which is more deterministic.
        SpecialPowers.getDOMWindowUtils(window).respectDisplayPortSuppression(test.dp_suppression);
      }

      function spawnTest(aFile) {
        w = window.open('', "_blank");
        w.subtestDone = advanceSubtestExecution;
        w.isApzSubtest = true;
        w.SimpleTest = SimpleTest;
        w.is = function(a, b, msg) { return is(a, b, aFile + " | " + msg); };
        w.ok = function(cond, name, diag) { return ok(cond, aFile + " | " + name, diag); };
        if (test.onload) {
          w.addEventListener('load', function(e) { test.onload(w); }, { once: true });
        }
        var subtestUrl = location.href.substring(0, location.href.lastIndexOf('/') + 1) + aFile;
        function urlResolves(url) {
          var request = new XMLHttpRequest();
          request.open('GET', url, false);
          request.send();
          return request.status !== 404;
        }
        if (!urlResolves(subtestUrl)) {
          SimpleTest.ok(false, "Subtest URL " + subtestUrl + " does not resolve. " +
              "Be sure it's present in the support-files section of mochitest.ini.");
          reject();
          return;
        }
        w.location = subtestUrl;
        return w;
      }

      if (test.prefs) {
        // Got some prefs for this subtest, push them
        SpecialPowers.pushPrefEnv({"set": test.prefs}, function() {
          w = spawnTest(test.file);
        });
      } else {
        w = spawnTest(test.file);
      }
    }

    advanceSubtestExecution();
  }).catch(function(e) {
    SimpleTest.ok(false, "Error occurred while running subtests: " + e);
  });
}

function pushPrefs(prefs) {
  return SpecialPowers.pushPrefEnv({'set': prefs});
}

async function waitUntilApzStable() {
  if (!SpecialPowers.isMainProcess()) {
    // We use this waitUntilApzStable function during test initialization
    // and for those scenarios we want to flush the parent-process layer
    // tree to the compositor and wait for that as well. That way we know
    // that not only is the content-process layer tree ready in the compositor,
    // the parent-process layer tree in the compositor has the appropriate
    // RefLayer pointing to the content-process layer tree.

    // Sadly this helper function cannot reuse any code from other places because
    // it must be totally self-contained to be shipped over to the parent process.
    function parentProcessFlush() {
      addMessageListener("apz-flush", function() {
        ChromeUtils.import("resource://gre/modules/Services.jsm");
        var topWin = Services.wm.getMostRecentWindow("navigator:browser");
        var topUtils = topWin.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);

        var repaintDone = function() {
          Services.obs.removeObserver(repaintDone, "apz-repaints-flushed");
          // send message back to content process
          sendAsyncMessage("apz-flush-done", null);
        };
        var flushRepaint = function() {
          if (topUtils.isMozAfterPaintPending) {
            topWin.addEventListener("MozAfterPaint", flushRepaint, { once: true });
            return;
          }

          Services.obs.addObserver(repaintDone, "apz-repaints-flushed");
          if (topUtils.flushApzRepaints()) {
            dump("Parent process: flushed APZ repaints, waiting for callback...\n");
          } else {
            dump("Parent process: flushing APZ repaints was a no-op, triggering callback directly...\n");
            repaintDone();
          }
        }

        // Flush APZ repaints, but wait until all the pending paints have been
        // sent.
        flushRepaint();
      });
    }

    // This is the first time waitUntilApzStable is being called, do initialization
    if (typeof waitUntilApzStable.chromeHelper == "undefined") {
      waitUntilApzStable.chromeHelper = SpecialPowers.loadChromeScript(parentProcessFlush);
      ApzCleanup.register(() => { waitUntilApzStable.chromeHelper.destroy(); });
    }

    // Actually trigger the parent-process flush and wait for it to finish
    waitUntilApzStable.chromeHelper.sendAsyncMessage("apz-flush", null);
    await waitUntilApzStable.chromeHelper.promiseOneMessage("apz-flush-done");
  }

  await SimpleTest.promiseFocus(window);
  await promiseAllPaintsDone();
  await promiseApzRepaintsFlushed();
}

// This function returns a promise that is resolved after at least one paint
// has been sent and processed by the compositor. This function can force
// such a paint to happen if none are pending. This is useful to run after
// the waitUntilApzStable() but before reading the compositor-side APZ test
// data, because the test data for the content layers id only gets populated
// on content layer tree updates *after* the root layer tree has a RefLayer
// pointing to the contnet layer tree. waitUntilApzStable itself guarantees
// that the root layer tree is pointing to the content layer tree, but does
// not guarantee the subsequent paint; this function does that job.
async function forceLayerTreeToCompositor() {
  var utils = SpecialPowers.getDOMWindowUtils(window);
  if (!utils.isMozAfterPaintPending) {
    dump("Forcing a paint since none was pending already...\n");
    var testMode = utils.isTestControllingRefreshes;
    utils.advanceTimeAndRefresh(0);
    if (!testMode) {
      utils.restoreNormalRefresh();
    }
  }
  await promiseAllPaintsDone(null, true);
  await promiseApzRepaintsFlushed();
}

function isApzEnabled() {
  var enabled = SpecialPowers.getDOMWindowUtils(window).asyncPanZoomEnabled;
  if (!enabled) {
    // All tests are required to have at least one assertion. Since APZ is
    // disabled, and the main test is presumably not going to run, we stick in
    // a dummy assertion here to keep the test passing.
    SimpleTest.ok(true, "APZ is not enabled; this test will be skipped");
  }
  return enabled;
}

function isKeyApzEnabled() {
  return isApzEnabled() && SpecialPowers.getBoolPref("apz.keyboard.enabled");
}

// Despite what this function name says, this does not *directly* run the
// provided continuation testFunction. Instead, it returns a function that
// can be used to run the continuation. The extra level of indirection allows
// it to be more easily added to a promise chain, like so:
//   waitUntilApzStable().then(runContinuation(myTest));
//
// If you want to run the continuation directly, outside of a promise chain,
// you can invoke the return value of this function, like so:
//   runContinuation(myTest)();
function runContinuation(testFunction) {
  // We need to wrap this in an extra function, so that the call site can
  // be more readable without running the promise too early. In other words,
  // if we didn't have this extra function, the promise would start running
  // during construction of the promise chain, concurrently with the first
  // promise in the chain.
  return function() {
    return new Promise(function(resolve, reject) {
      var testContinuation = null;

      function driveTest() {
        if (!testContinuation) {
          testContinuation = testFunction(driveTest);
        }
        var ret = testContinuation.next();
        if (ret.done) {
          resolve();
        }
      }

      driveTest();
    });
  };
}

// Take a snapshot of the given rect, *including compositor transforms* (i.e.
// includes async scroll transforms applied by APZ). If you don't need the
// compositor transforms, you can probably get away with using
// SpecialPowers.snapshotWindowWithOptions or one of the friendlier wrappers.
// The rect provided is expected to be relative to the screen, for example as
// returned by rectRelativeToScreen in apz_test_native_event_utils.js.
// Example usage:
//   var snapshot = getSnapshot(rectRelativeToScreen(myDiv));
// which will take a snapshot of the 'myDiv' element. Note that if part of the
// element is obscured by other things on top, the snapshot will include those
// things. If it is clipped by a scroll container, the snapshot will include
// that area anyway, so you will probably get parts of the scroll container in
// the snapshot. If the rect extends outside the browser window then the
// results are undefined.
// The snapshot is returned in the form of a data URL.
function getSnapshot(rect) {
  function parentProcessSnapshot() {
    addMessageListener('snapshot', function(rect) {
      ChromeUtils.import('resource://gre/modules/Services.jsm');
      var topWin = Services.wm.getMostRecentWindow('navigator:browser');

      // reposition the rect relative to the top-level browser window
      rect = JSON.parse(rect);
      rect.x -= topWin.mozInnerScreenX;
      rect.y -= topWin.mozInnerScreenY;

      // take the snapshot
      var canvas = topWin.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
      canvas.width = rect.w;
      canvas.height = rect.h;
      var ctx = canvas.getContext("2d");
      ctx.drawWindow(topWin, rect.x, rect.y, rect.w, rect.h, 'rgb(255,255,255)', ctx.DRAWWINDOW_DRAW_VIEW | ctx.DRAWWINDOW_USE_WIDGET_LAYERS | ctx.DRAWWINDOW_DRAW_CARET);
      return canvas.toDataURL();
    });
  }

  if (typeof getSnapshot.chromeHelper == 'undefined') {
    // This is the first time getSnapshot is being called; do initialization
    getSnapshot.chromeHelper = SpecialPowers.loadChromeScript(parentProcessSnapshot);
    ApzCleanup.register(function() { getSnapshot.chromeHelper.destroy() });
  }

  return getSnapshot.chromeHelper.sendSyncMessage('snapshot', JSON.stringify(rect)).toString();
}

// Takes the document's query string and parses it, assuming the query string
// is composed of key-value pairs where the value is in JSON format. The object
// returned contains the various values indexed by their respective keys. In
// case of duplicate keys, the last value be used.
// Examples:
//   ?key="value"&key2=false&key3=500
//     produces { "key": "value", "key2": false, "key3": 500 }
//   ?key={"x":0,"y":50}&key2=[1,2,true]
//     produces { "key": { "x": 0, "y": 0 }, "key2": [1, 2, true] }
function getQueryArgs() {
  var args = {};
  if (location.search.length > 0) {
    var params = location.search.substr(1).split('&');
    for (var p of params) {
      var [k, v] = p.split('=');
      args[k] = JSON.parse(v);
    }
  }
  return args;
}

// Return a function that returns a promise to create a script element with the
// given URI and append it to the head of the document in the given window.
// As with runContinuation(), the extra function wrapper is for convenience
// at the call site, so that this can be chained with other promises:
//   waitUntilApzStable().then(injectScript('foo'))
//                       .then(injectScript('bar'));
// If you want to do the injection right away, run the function returned by
// this function:
//   injectScript('foo')();
function injectScript(aScript, aWindow = window) {
  return function() {
    return new Promise(function(resolve, reject) {
      var e = aWindow.document.createElement('script');
      e.type = 'text/javascript';
      e.onload = function() {
        resolve();
      };
      e.onerror = function() {
        dump('Script [' + aScript + '] errored out\n');
        reject();
      };
      e.src = aScript;
      aWindow.document.getElementsByTagName('head')[0].appendChild(e);
    });
  };
}

// Compute some configuration information used for hit testing.
// The computed information is cached to avoid recomputing it
// each time this function is called.
// The computed information is an object with three fields:
//   utils: the nsIDOMWindowUtils instance for this window
//   isWebRender: true if WebRender is enabled
//   isWindow: true if the platform is Windows
function getHitTestConfig() {
  if (!("hitTestConfig" in window)) {
    var utils = SpecialPowers.getDOMWindowUtils(window);
    var isWebRender = (utils.layerManagerType == 'WebRender');
    var isWindows = (getPlatform() == 'windows');
    window.hitTestConfig = { utils, isWebRender, isWindows };
  }
  return window.hitTestConfig;
}

// Compute the coordinates of the center of the given element. The argument
// can either be a string (the id of the element desired) or the element
// itself.
function centerOf(element) {
  if (typeof element === "string") {
    element = document.getElementById(element);
  }
  var bounds = element.getBoundingClientRect();
  return { x: bounds.x + (bounds.width / 2), y: bounds.y + (bounds.height / 2) };
}

// Peform a compositor hit test at the given point and return the result.
// The returned object has two fields:
//   hitInfo: a combination of APZHitResultFlags
//   scrollId: the view-id of the scroll frame that was hit
function hitTest(point) {
  var utils = getHitTestConfig().utils;
  dump("Hit-testing point (" + point.x + ", " + point.y + ")\n");
  utils.sendMouseEvent("MozMouseHittest", point.x, point.y, 0, 0, 0, true, 0, 0, true, true);
  var data = utils.getCompositorAPZTestData();
  ok(data.hitResults.length >= 1, "Expected at least one hit result in the APZTestData");
  var result = data.hitResults[data.hitResults.length - 1];
  return { hitInfo: result.hitResult, scrollId: result.scrollId };
}

// Returns a canonical stringification of the hitInfo bitfield.
function hitInfoToString(hitInfo) {
  var strs = [];
  for (var flag in APZHitResultFlags) {
    if ((hitInfo & APZHitResultFlags[flag]) != 0) {
      strs.push(flag);
    }
  }
  if (strs.length == 0) {
    return "INVISIBLE";
  }
  strs.sort(function(a, b) {
    return APZHitResultFlags[a] - APZHitResultFlags[b];
  });
  return strs.join(" | ");
}

// Takes an object returned by hitTest, along with the expected values, and
// asserts that they match. Notably, it uses hitInfoToString to provide a
// more useful message for the case that the hit info doesn't match
function checkHitResult(hitResult, expectedHitInfo, expectedScrollId, desc) {
  is(hitInfoToString(hitResult.hitInfo), hitInfoToString(expectedHitInfo), desc + " hit info");
  is(hitResult.scrollId, expectedScrollId, desc + " scrollid");
}

// Symbolic constants used by hitTestScrollbar().
var ScrollbarTrackLocation = {
    START: 1,
    END: 2
};
var LayerState = {
    ACTIVE: 1,
    INACTIVE: 2
};

// Perform a hit test on the scrollbar(s) of a scroll frame.
// This function takes a single argument which is expected to be
// an object with the following fields:
//   element: The scroll frame to perform the hit test on.
//   directions: The direction(s) of scrollbars to test.
//     If directions.vertical is true, the vertical scrollbar will be tested.
//     If directions.horizontal is true, the horizontal scrollbar will be tested.
//     Both may be true in a single call (in which case two tests are performed).
//   expectedScrollId: The scroll id that is expected to be hit.
//   trackLocation: One of ScrollbarTrackLocation.{START, END}.
//     Determines which end of the scrollbar track is targeted.
//   expectThumb: Whether the scrollbar thumb is expected to be present
//     at the targeted end of the scrollbar track.
//   layerState: Whether the scroll frame is active or inactive.
// The function performs the hit tests and asserts that the returned
// hit test information is consistent with the passed parameters.
// There is no return value.
// Tests that use this function must set the pref
// "layout.scrollbars.always-layerize-track".
function hitTestScrollbar(params) {
  var config = getHitTestConfig();

  var elem = params.element;

  var boundingClientRect = elem.getBoundingClientRect();

  var verticalScrollbarWidth = boundingClientRect.width - elem.clientWidth;
  var horizontalScrollbarHeight = boundingClientRect.height - elem.clientHeight;

  // On windows, the scrollbar tracks have buttons on the end. When computing
  // coordinates for hit-testing we need to account for this. We assume the
  // buttons are square, and so can use the scrollbar width/height to estimate
  // the size of the buttons
  var scrollbarArrowButtonHeight = config.isWindows ? verticalScrollbarWidth : 0;
  var scrollbarArrowButtonWidth = config.isWindows ? horizontalScrollbarHeight : 0;

  // Compute the expected hit result flags.
  // The direction flag (APZHitResultFlags.SCROLLBAR_VERTICAL) is added in
  // later, for the vertical test only.
  // The APZHitResultFlags.SCROLLBAR flag will be present regardless of whether
  // the layer is active or inactive because we force layerization of scrollbar
  // tracks. Unfortunately not forcing the layerization results in different
  // behaviour on different platforms which makes testing harder.
  var expectedHitInfo = APZHitResultFlags.VISIBLE | APZHitResultFlags.SCROLLBAR;
  if (params.expectThumb) {
    // We do not generate the layers for thumbs on inactive scrollframes.
    expectedHitInfo |= APZHitResultFlags.DISPATCH_TO_CONTENT;
    if (params.layerState == LayerState.ACTIVE) {
      expectedHitInfo |= APZHitResultFlags.SCROLLBAR_THUMB;
    }
  }

  var scrollframeMsg = (params.layerState == LayerState.ACTIVE)
    ? "active scrollframe" : "inactive scrollframe";

  // Hit-test the targeted areas, assuming we don't have overlay scrollbars
  // with zero dimensions.
  if (params.directions.vertical && verticalScrollbarWidth > 0) {
    var verticalScrollbarPoint = {
        x: boundingClientRect.right - (verticalScrollbarWidth / 2),
        y: (params.trackLocation == ScrollbarTrackLocation.START)
             ? (boundingClientRect.y + scrollbarArrowButtonHeight + 5)
             : (boundingClientRect.bottom - horizontalScrollbarHeight - scrollbarArrowButtonHeight - 5)
    };
    checkHitResult(hitTest(verticalScrollbarPoint),
                   expectedHitInfo | APZHitResultFlags.SCROLLBAR_VERTICAL,
                   params.expectedScrollId,
                   scrollframeMsg + " - vertical scrollbar");
  }

  if (params.directions.horizontal && horizontalScrollbarHeight > 0) {
    var horizontalScrollbarPoint = {
        x: (params.trackLocation == ScrollbarTrackLocation.START)
             ? (boundingClientRect.x + scrollbarArrowButtonWidth + 5)
             : (boundingClientRect.right - verticalScrollbarWidth - scrollbarArrowButtonWidth - 5),
        y: boundingClientRect.bottom - (horizontalScrollbarHeight / 2),
    };
    checkHitResult(hitTest(horizontalScrollbarPoint),
                   expectedHitInfo,
                   params.expectedScrollId,
                   scrollframeMsg + " - horizontal scrollbar");
  }
}

// Return a list of prefs for the given test identifier.
function getPrefs(ident) {
  switch (ident) {
    case "TOUCH_EVENTS:PAN":
      return [
        // Dropping the touch slop to 0 makes the tests easier to write because
        // we can just do a one-pixel drag to get over the pan threshold rather
        // than having to hard-code some larger value.
        ["apz.touch_start_tolerance", "0.0"],
        // The touchstart from the drag can turn into a long-tap if the touch-move
        // events get held up. Try to prevent that by making long-taps require
        // a 10 second hold. Note that we also cannot enable chaos mode on this
        // test for this reason, since chaos mode can cause the long-press timer
        // to fire sooner than the pref dictates.
        ["ui.click_hold_context_menus.delay", 10000],
        // The subtests in this test do touch-drags to pan the page, but we don't
        // want those pans to turn into fling animations, so we increase the
        // fling min velocity requirement absurdly high.
        ["apz.fling_min_velocity_threshold", "10000"],
        // The helper_div_pan's div gets a displayport on scroll, but if the
        // test takes too long the displayport can expire before the new scroll
        // position is synced back to the main thread. So we disable displayport
        // expiry for these tests.
        ["apz.displayport_expiry_ms", 0],
      ];
    default:
      return [];
  }
}

var ApzCleanup = {
  _cleanups: [],

  register: function(func) {
    if (this._cleanups.length == 0) {
      if (!window.isApzSubtest) {
        SimpleTest.registerCleanupFunction(this.execute.bind(this));
      } // else ApzCleanup.execute is called from runSubtestsSeriallyInFreshWindows
    }
    this._cleanups.push(func);
  },

  execute: function() {
    while (this._cleanups.length > 0) {
      var func = this._cleanups.pop();
      try {
        func();
      } catch (ex) {
        SimpleTest.ok(false, "Subtest cleanup function [" + func.toString() + "] threw exception [" + ex + "] on page [" + location.href + "]");
      }
    }
  }
};
