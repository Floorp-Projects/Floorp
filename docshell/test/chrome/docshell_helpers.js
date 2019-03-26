/**
 * Import common SimpleTest methods so that they're usable in this window.
 */
var imports = [ "SimpleTest", "is", "isnot", "ok", "onerror", "todo",
  "todo_is", "todo_isnot" ];
for (var name of imports) {
  window[name] = window.opener.wrappedJSObject[name];
}
const {BrowserTestUtils} = ChromeUtils.import("resource://testing-common/BrowserTestUtils.jsm");
var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Some functions assume chrome-harness.js has been loaded.
/* import-globals-from ../../../testing/mochitest/chrome-harness.js */

/**
 * Define global constants and variables.
 */
const NAV_NONE = 0;
const NAV_BACK = 1;
const NAV_FORWARD = 2;
const NAV_URI = 3;
const NAV_RELOAD = 4;

var gExpectedEvents; // an array of events which are expected to
                              // be triggered by this navigation
var gUnexpectedEvents; // an array of event names which are NOT expected
                              // to be triggered by this navigation
var gFinalEvent; // true if the last expected event has fired
var gUrisNotInBFCache = []; // an array of uri's which shouldn't be stored
                              // in the bfcache
var gNavType = NAV_NONE; // defines the most recent navigation type
                              // executed by doPageNavigation
var gOrigMaxTotalViewers =    // original value of max_total_viewers,
  undefined; // to be restored at end of test

var gExtractedPath = null; // used to cache file path for extracting files from a .jar file

/**
 * The doPageNavigation() function performs page navigations asynchronously,
 * listens for specified events, and compares actual events with a list of
 * expected events.  When all expected events have occurred, an optional
 * callback can be notified. The parameter passed to this function is an
 * object with the following properties:
 *
 *                uri: if !undefined, the browser will navigate to this uri
 *
 *               back: if true, the browser will execute goBack()
 *
 *            forward: if true, the browser will execute goForward()
 *
 *             reload: if true, the browser will execute reload()
 *
 *  eventsToListenFor: an array containing one or more of the following event
 *                     types to listen for:  "pageshow", "pagehide", "onload",
 *                     "onunload".  If this property is undefined, only a
 *                     single "pageshow" events will be listened for.  If this
 *                     property is explicitly empty, [], then no events will
 *                     be listened for.
 *
 *     expectedEvents: an array of one or more expectedEvent objects,
 *                     corresponding to the events which are expected to be
 *                     fired for this navigation.  Each object has the
 *                     following properties:
 *
 *                          type: one of the event type strings
 *                          title (optional): the title of the window the
 *                              event belongs to
 *                          persisted (optional): the event's expected
 *                              .persisted attribute
 *
 *                     This function will verify that events with the
 *                     specified properties are fired in the same order as
 *                     specified in the array.  If .title or .persisted
 *                     properties for an expectedEvent are undefined, those
 *                     properties will not be verified for that particular
 *                     event.
 *
 *                     This property is ignored if eventsToListenFor is
 *                     undefined or [].
 *
 *     preventBFCache: if true, an unload handler will be added to the loaded
 *                     page to prevent it from being bfcached.  This property
 *                     has no effect when eventsToListenFor is [].
 *
 *      onNavComplete: a callback which is notified after all expected events
 *                     have occurred, or after a timeout has elapsed.  This
 *                     callback is not notified if eventsToListenFor is [].
 *
 * There must be an expectedEvent object for each event of the types in
 * eventsToListenFor which is triggered by this navigation.  For example, if
 * eventsToListenFor = [ "pagehide", "pageshow" ], then expectedEvents
 * must contain an object for each pagehide and pageshow event which occurs as
 * a result of this navigation.
 */
// eslint-disable-next-line complexity
function doPageNavigation(params) {
  // Parse the parameters.
  let back = params.back ? params.back : false;
  let forward = params.forward ? params.forward : false;
  let reload = params.reload ? params.reload : false;
  let uri = params.uri ? params.uri : false;
  let eventsToListenFor = typeof(params.eventsToListenFor) != "undefined" ?
    params.eventsToListenFor : ["pageshow"];
  gExpectedEvents = typeof(params.eventsToListenFor) == "undefined" ||
    eventsToListenFor.length == 0 ? undefined : params.expectedEvents;
  gUnexpectedEvents = typeof(params.eventsToListenFor) == "undefined" ||
    eventsToListenFor.length == 0 ? undefined : params.unexpectedEvents;
  let preventBFCache = (typeof[params.preventBFCache] == "undefined") ?
    false : params.preventBFCache;
  let waitOnly = (typeof(params.waitForEventsOnly) == "boolean"
    && params.waitForEventsOnly);

  // Do some sanity checking on arguments.
  if (back && forward)
    throw new Error("Can't specify both back and forward");
  if (back && uri)
    throw new Error("Can't specify both back and a uri");
  if (forward && uri)
    throw new Error("Can't specify both forward and a uri");
  if (reload && (forward || back || uri))
    throw new Error("Can't specify reload and another navigation type");
  if (!back && !forward && !uri && !reload && !waitOnly)
    throw new Error("Must specify back or foward or reload or uri");
  if (params.onNavComplete && eventsToListenFor.length == 0)
    throw new Error("Can't use onNavComplete when eventsToListenFor == []");
  if (params.preventBFCache && eventsToListenFor.length == 0)
    throw new Error("Can't use preventBFCache when eventsToListenFor == []");
  if (params.preventBFCache && waitOnly)
    throw new Error("Can't prevent bfcaching when only waiting for events");
  if (waitOnly && typeof(params.onNavComplete) == "undefined")
    throw new Error("Must specify onNavComplete when specifying waitForEventsOnly");
  if (waitOnly && (back || forward || reload || uri))
    throw new Error("Can't specify a navigation type when using waitForEventsOnly");
  for (let anEventType of eventsToListenFor) {
    let eventFound = false;
    if ( (anEventType == "pageshow") && (!gExpectedEvents) )
      eventFound = true;
    if (gExpectedEvents) {
      for (let anExpectedEvent of gExpectedEvents) {
        if (anExpectedEvent.type == anEventType)
          eventFound = true;
      }
    }
    if (gUnexpectedEvents) {
      for (let anExpectedEventType of gUnexpectedEvents) {
        if (anExpectedEventType == anEventType)
          eventFound = true;
      }
    }
    if (!eventFound)
      throw new Error(`Event type ${anEventType} is specified in ` +
                      "eventsToListenFor, but not in expectedEvents");
  }

  // If the test explicitly sets .eventsToListenFor to [], don't wait for any
  // events.
  gFinalEvent = eventsToListenFor.length == 0;

  // Add an event listener for each type of event in the .eventsToListenFor
  // property of the input parameters.
  for (let eventType of eventsToListenFor) {
    dump("TEST: registering a listener for " + eventType + " events\n");
    TestWindow.getBrowser().addEventListener(eventType, pageEventListener,
      true);
  }

  // Perform the specified navigation.
  if (back) {
    gNavType = NAV_BACK;
    TestWindow.getBrowser().goBack();
  } else if (forward) {
    gNavType = NAV_FORWARD;
    TestWindow.getBrowser().goForward();
  } else if (uri) {
    gNavType = NAV_URI;
    BrowserTestUtils.loadURI(TestWindow.getBrowser(), uri);
  } else if (reload) {
    gNavType = NAV_RELOAD;
    TestWindow.getBrowser().reload();
  } else if (waitOnly) {
    gNavType = NAV_NONE;
  } else {
    throw new Error("No valid navigation type passed to doPageNavigation!");
  }

  // If we're listening for events and there is an .onNavComplete callback,
  // wait for all events to occur, and then call doPageNavigation_complete().
  if (eventsToListenFor.length > 0 && params.onNavComplete) {
    waitForTrue(
      function() { return gFinalEvent; },
      function() {
        doPageNavigation_complete(eventsToListenFor, params.onNavComplete,
          preventBFCache);
      } );
  }
}

/**
 * Finish doPageNavigation(), by removing event listeners, adding an unload
 * handler if appropriate, and calling the onNavComplete callback.  This
 * function is called after all the expected events for this navigation have
 * occurred.
 */
function doPageNavigation_complete(eventsToListenFor, onNavComplete,
  preventBFCache) {
  // Unregister our event listeners.
  dump("TEST: removing event listeners\n");
  for (let eventType of eventsToListenFor) {
    TestWindow.getBrowser().removeEventListener(eventType, pageEventListener,
      true);
  }

  // If the .preventBFCache property was set, add an empty unload handler to
  // prevent the page from being bfcached.
  let uri = TestWindow.getBrowser().currentURI.spec;
  if (preventBFCache) {
    TestWindow.getWindow().addEventListener("unload", function() {
        dump("TEST: Called dummy unload function to prevent page from " +
          "being bfcached.\n");
      }, true);

    // Save the current uri in an array of uri's which shouldn't be
    // stored in the bfcache, for later verification.
    if (!(uri in gUrisNotInBFCache)) {
      gUrisNotInBFCache.push(uri);
    }
  } else if (gNavType == NAV_URI) {
    // If we're navigating to a uri and .preventBFCache was not
    // specified, splice it out of gUrisNotInBFCache if it's there.
    gUrisNotInBFCache.forEach(
      function(element, index, array) {
        if (element == uri) {
          array.splice(index, 1);
        }
      }, this);
  }

  // Notify the callback now that we're done.
  onNavComplete.call();
}

/**
 * Allows a test to wait for page navigation events, and notify a
 * callback when they've all been received.  This works exactly the
 * same as doPageNavigation(), except that no navigation is initiated.
 */
function waitForPageEvents(params) {
  params.waitForEventsOnly = true;
  doPageNavigation(params);
}

/**
 * The event listener which listens for expectedEvents.
 */
function pageEventListener(event) {
  try {
    dump("TEST: eventListener received a " + event.type + " event for page " +
      event.originalTarget.title + ", persisted=" + event.persisted + "\n");
  } catch (e) {
    // Ignore any exception.
  }

  // If this page shouldn't be in the bfcache because it was previously
  // loaded with .preventBFCache, make sure that its pageshow event
  // has .persisted = false, even if the test doesn't explicitly test
  // for .persisted.
  if ( (event.type == "pageshow") &&
    (gNavType == NAV_BACK || gNavType == NAV_FORWARD) ) {
    let uri = TestWindow.getBrowser().currentURI.spec;
    if (uri in gUrisNotInBFCache) {
      ok(!event.persisted, "pageshow event has .persisted = false, even " +
       "though it was loaded with .preventBFCache previously\n");
    }
  }

  if (typeof(gUnexpectedEvents) != "undefined") {
    is(gUnexpectedEvents.indexOf(event.type), -1,
       "Should not get unexpected event " + event.type);
  }

  // If no expected events were specified, mark the final event as having been
  // triggered when a pageshow event is fired; this will allow
  // doPageNavigation() to return.
  if ((typeof(gExpectedEvents) == "undefined") && event.type == "pageshow") {
    waitForNextPaint(function() { gFinalEvent = true; });
    return;
  }

  // If there are explicitly no expected events, but we receive one, it's an
  // error.
  if (gExpectedEvents.length == 0) {
    ok(false, "Unexpected event (" + event.type + ") occurred");
    return;
  }

  // Grab the next expected event, and compare its attributes against the
  // actual event.
  let expected = gExpectedEvents.shift();

  is(event.type, expected.type,
    "A " + expected.type + " event was expected, but a " +
    event.type + " event occurred");

  if (typeof(expected.title) != "undefined") {
    ok(event.originalTarget instanceof HTMLDocument,
       "originalTarget for last " + event.type +
       " event not an HTMLDocument");
    is(event.originalTarget.title, expected.title,
      "A " + event.type + " event was expected for page " +
      expected.title + ", but was fired for page " +
      event.originalTarget.title);
  }

  if (typeof(expected.persisted) != "undefined") {
    is(event.persisted, expected.persisted,
      "The persisted property of the " + event.type + " event on page " +
      event.originalTarget.location + " had an unexpected value");
  }

  if ("visibilityState" in expected) {
    is(event.originalTarget.visibilityState, expected.visibilityState,
       "The visibilityState property of the document on page " +
       event.originalTarget.location + " had an unexpected value");
  }

  if ("hidden" in expected) {
    is(event.originalTarget.hidden, expected.hidden,
       "The hidden property of the document on page " +
       event.originalTarget.location + " had an unexpected value");
  }

  // If we're out of expected events, let doPageNavigation() return.
  if (gExpectedEvents.length == 0)
    waitForNextPaint(function() { gFinalEvent = true; });
}

/**
 * End a test.
 */
function finish() {
  // Work around bug 467960.
  var history = TestWindow.getBrowser().webNavigation.sessionHistory;
  history.legacySHistory.PurgeHistory(history.count);

  // If the test changed the value of max_total_viewers via a call to
  // enableBFCache(), then restore it now.
  if (typeof(gOrigMaxTotalViewers) != "undefined") {
    Services.prefs.setIntPref("browser.sessionhistory.max_total_viewers",
      gOrigMaxTotalViewers);
  }

  // Close the test window and signal the framework that the test is done.
  let opener = window.opener;
  let SimpleTest = opener.wrappedJSObject.SimpleTest;

  // Wait for the window to be closed before finishing the test
  Services.ww.registerNotification(function observer(subject, topic, data) {
    if (topic == "domwindowclosed") {
      Services.ww.unregisterNotification(observer);
      SimpleTest.waitForFocus(SimpleTest.finish, opener);
    }
  });

  window.close();
}

/**
 * Helper function which waits until another function returns true, or until a
 * timeout occurs, and then notifies a callback.
 *
 * Parameters:
 *
 *    fn: a function which is evaluated repeatedly, and when it turns true,
 *        the onWaitComplete callback is notified.
 *
 *    onWaitComplete:  a callback which will be notified when fn() returns
 *        true, or when a timeout occurs.
 *
 *    timeout: a timeout, in seconds or ms, after which waitForTrue() will
 *        fail an assertion and then return, even if the fn function never
 *        returns true.  If timeout is undefined, waitForTrue() will never
 *        time out.
 */
function waitForTrue(fn, onWaitComplete, timeout) {
  var start = new Date().valueOf();
  if (typeof(timeout) != "undefined") {
    // If timeoutWait is less than 500, assume it represents seconds, and
    // convert to ms.
    if (timeout < 500)
      timeout *= 1000;
  }

  // Loop until the test function returns true, or until a timeout occurs,
  // if a timeout is defined.
  var intervalid;
  intervalid =
    setInterval(
      function() {
        var timeoutHit = false;
        if (typeof(timeout) != "undefined") {
          timeoutHit = new Date().valueOf() - start >=
            timeout;
          if (timeoutHit) {
            ok(false, "Timed out waiting for condition");
          }
        }
        if (timeoutHit || fn.call()) {
          // Stop calling the test function and notify the callback.
          clearInterval(intervalid);
          onWaitComplete.call();
        }
      }, 20);
}

function waitForNextPaint(cb) {
  requestAnimationFrame(_ => requestAnimationFrame(cb));
}

/**
 * Enable or disable the bfcache.
 *
 * Parameters:
 *
 *   enable: if true, set max_total_viewers to -1 (the default); if false, set
 *           to 0 (disabled), if a number, set it to that specific number
 */
function enableBFCache(enable) {
  // If this is the first time the test called enableBFCache(),
  // store the original value of max_total_viewers, so it can
  // be restored at the end of the test.
  if (typeof(gOrigMaxTotalViewers) == "undefined") {
    gOrigMaxTotalViewers =
      Services.prefs.getIntPref("browser.sessionhistory.max_total_viewers");
  }

  if (typeof(enable) == "boolean") {
    if (enable)
      Services.prefs.setIntPref("browser.sessionhistory.max_total_viewers", -1);
    else
      Services.prefs.setIntPref("browser.sessionhistory.max_total_viewers", 0);
  } else if (typeof(enable) == "number") {
    Services.prefs.setIntPref("browser.sessionhistory.max_total_viewers", enable);
  }
}

/*
 * get http root for local tests.  Use a single extractJarToTmp instead of
 * extracting for each test.
 * Returns a file://path if we have a .jar file
 */
function getHttpRoot() {
  var location = window.location.href;
  location = getRootDirectory(location);
  var jar = getJar(location);
  if (jar != null) {
    if (gExtractedPath == null) {
      var resolved = extractJarToTmp(jar);
      gExtractedPath = resolved.path;
    }
  } else {
    return null;
  }
  return "file://" + gExtractedPath + "/";
}

/**
 * Returns the full HTTP url for a file in the mochitest docshell test
 * directory.
 */
function getHttpUrl(filename) {
  var root = getHttpRoot();
  if (root == null) {
    root = "http://mochi.test:8888/chrome/docshell/test/chrome/";
  }
  return root + filename;
}

/**
 * A convenience object with methods that return the current test window,
 * browser, and document.
 */
var TestWindow = {};
TestWindow.getWindow = function() {
  return document.getElementById("content").contentWindow;
};
TestWindow.getBrowser = function() {
  return document.getElementById("content");
};
TestWindow.getDocument = function() {
  return document.getElementById("content").contentDocument;
};
