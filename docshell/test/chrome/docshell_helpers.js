if (!window.opener && window.arguments) {
  window.opener = window.arguments[0];
}
/**
 * Import common SimpleTest methods so that they're usable in this window.
 */
/* globals SimpleTest, is, isnot, ok, onerror, todo, todo_is, todo_isnot */
var imports = [
  "SimpleTest",
  "is",
  "isnot",
  "ok",
  "onerror",
  "todo",
  "todo_is",
  "todo_isnot",
];
for (var name of imports) {
  window[name] = window.opener.wrappedJSObject[name];
}
const { BrowserTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/BrowserTestUtils.sys.mjs"
);

const ACTOR_MODULE_URI =
  "chrome://mochitests/content/chrome/docshell/test/chrome/DocShellHelpers.sys.mjs";
const { DocShellHelpersParent } = ChromeUtils.importESModule(ACTOR_MODULE_URI);
// Some functions assume chrome-harness.js has been loaded.
/* import-globals-from ../../../testing/mochitest/chrome-harness.js */

/**
 * Define global constants and variables.
 */
const NAV_NONE = 0;
const NAV_BACK = 1;
const NAV_FORWARD = 2;
const NAV_GOTOINDEX = 3;
const NAV_URI = 4;
const NAV_RELOAD = 5;

var gExpectedEvents; // an array of events which are expected to
// be triggered by this navigation
var gUnexpectedEvents; // an array of event names which are NOT expected
// to be triggered by this navigation
var gFinalEvent; // true if the last expected event has fired
var gUrisNotInBFCache = []; // an array of uri's which shouldn't be stored
// in the bfcache
var gNavType = NAV_NONE; // defines the most recent navigation type
// executed by doPageNavigation
var gOrigMaxTotalViewers = undefined; // original value of max_total_viewers, // to be restored at end of test

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
 *          gotoIndex: if a number, the browser will execute gotoIndex() with
 *                     the number as index
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
 *     preventBFCache: if true, an RTCPeerConnection will be added to the loaded
 *                     page to prevent it from being bfcached.  This property
 *                     has no effect when eventsToListenFor is [].
 *
 *      onNavComplete: a callback which is notified after all expected events
 *                     have occurred, or after a timeout has elapsed.  This
 *                     callback is not notified if eventsToListenFor is [].
 *   onGlobalCreation: a callback which is notified when a DOMWindow is created
 *                     (implemented by observing
 *                     "content-document-global-created")
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
  let gotoIndex = params.gotoIndex ? params.gotoIndex : false;
  let reload = params.reload ? params.reload : false;
  let uri = params.uri ? params.uri : false;
  let eventsToListenFor =
    typeof params.eventsToListenFor != "undefined"
      ? params.eventsToListenFor
      : ["pageshow"];
  gExpectedEvents =
    typeof params.eventsToListenFor == "undefined" || !eventsToListenFor.length
      ? undefined
      : params.expectedEvents;
  gUnexpectedEvents =
    typeof params.eventsToListenFor == "undefined" || !eventsToListenFor.length
      ? undefined
      : params.unexpectedEvents;
  let preventBFCache =
    typeof [params.preventBFCache] == "undefined"
      ? false
      : params.preventBFCache;
  let waitOnly =
    typeof params.waitForEventsOnly == "boolean" && params.waitForEventsOnly;

  // Do some sanity checking on arguments.
  let navigation = ["back", "forward", "gotoIndex", "reload", "uri"].filter(k =>
    params.hasOwnProperty(k)
  );
  if (navigation.length > 1) {
    throw new Error(`Can't specify both ${navigation[0]} and ${navigation[1]}`);
  } else if (!navigation.length && !waitOnly) {
    throw new Error(
      "Must specify back or forward or gotoIndex or reload or uri"
    );
  }
  if (params.onNavComplete && !eventsToListenFor.length) {
    throw new Error("Can't use onNavComplete when eventsToListenFor == []");
  }
  if (params.preventBFCache && !eventsToListenFor.length) {
    throw new Error("Can't use preventBFCache when eventsToListenFor == []");
  }
  if (params.preventBFCache && waitOnly) {
    throw new Error("Can't prevent bfcaching when only waiting for events");
  }
  if (waitOnly && typeof params.onNavComplete == "undefined") {
    throw new Error(
      "Must specify onNavComplete when specifying waitForEventsOnly"
    );
  }
  if (waitOnly && navigation.length) {
    throw new Error(
      "Can't specify a navigation type when using waitForEventsOnly"
    );
  }
  for (let anEventType of eventsToListenFor) {
    let eventFound = false;
    if (anEventType == "pageshow" && !gExpectedEvents) {
      eventFound = true;
    }
    if (gExpectedEvents) {
      for (let anExpectedEvent of gExpectedEvents) {
        if (anExpectedEvent.type == anEventType) {
          eventFound = true;
        }
      }
    }
    if (gUnexpectedEvents) {
      for (let anExpectedEventType of gUnexpectedEvents) {
        if (anExpectedEventType == anEventType) {
          eventFound = true;
        }
      }
    }
    if (!eventFound) {
      throw new Error(
        `Event type ${anEventType} is specified in ` +
          "eventsToListenFor, but not in expectedEvents"
      );
    }
  }

  // If the test explicitly sets .eventsToListenFor to [], don't wait for any
  // events.
  gFinalEvent = !eventsToListenFor.length;

  // Add observers as needed.
  let observers = new Map();
  if (params.hasOwnProperty("onGlobalCreation")) {
    observers.set("content-document-global-created", params.onGlobalCreation);
  }

  // Add an event listener for each type of event in the .eventsToListenFor
  // property of the input parameters, and add an observer for all the topics
  // in the observers map.
  let cleanup;
  let useActor = TestWindow.getBrowser().isRemoteBrowser;
  if (useActor) {
    ChromeUtils.registerWindowActor("DocShellHelpers", {
      parent: {
        esModuleURI: ACTOR_MODULE_URI,
      },
      child: {
        esModuleURI: ACTOR_MODULE_URI,
        events: {
          pageshow: { createActor: true, capture: true },
          pagehide: { createActor: true, capture: true },
          load: { createActor: true, capture: true },
          unload: { createActor: true, capture: true },
          visibilitychange: { createActor: true, capture: true },
        },
        observers: observers.keys(),
      },
      allFrames: true,
    });
    DocShellHelpersParent.eventsToListenFor = eventsToListenFor;
    DocShellHelpersParent.observers = observers;

    cleanup = () => {
      DocShellHelpersParent.eventsToListenFor = null;
      DocShellHelpersParent.observers = null;
      ChromeUtils.unregisterWindowActor("DocShellHelpers");
    };
  } else {
    for (let eventType of eventsToListenFor) {
      dump("TEST: registering a listener for " + eventType + " events\n");
      TestWindow.getBrowser().addEventListener(
        eventType,
        pageEventListener,
        true
      );
    }
    if (observers.size > 0) {
      let observer = (_, topic) => {
        observers.get(topic).call();
      };
      for (let topic of observers.keys()) {
        Services.obs.addObserver(observer, topic);
      }

      // We only need to do cleanup for the observer, the event listeners will
      // go away with the window.
      cleanup = () => {
        for (let topic of observers.keys()) {
          Services.obs.removeObserver(observer, topic);
        }
      };
    }
  }

  if (cleanup) {
    // Register a cleanup function on domwindowclosed, to avoid contaminating
    // other tests if we bail out early because of an error.
    Services.ww.registerNotification(function windowClosed(
      subject,
      topic,
      data
    ) {
      if (topic == "domwindowclosed" && subject == window) {
        Services.ww.unregisterNotification(windowClosed);
        cleanup();
      }
    });
  }

  // Perform the specified navigation.
  if (back) {
    gNavType = NAV_BACK;
    TestWindow.getBrowser().goBack();
  } else if (forward) {
    gNavType = NAV_FORWARD;
    TestWindow.getBrowser().goForward();
  } else if (typeof gotoIndex == "number") {
    gNavType = NAV_GOTOINDEX;
    TestWindow.getBrowser().gotoIndex(gotoIndex);
  } else if (uri) {
    gNavType = NAV_URI;
    BrowserTestUtils.startLoadingURIString(TestWindow.getBrowser(), uri);
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
  if (eventsToListenFor.length && params.onNavComplete) {
    waitForTrue(
      function () {
        return gFinalEvent;
      },
      function () {
        doPageNavigation_complete(
          eventsToListenFor,
          params.onNavComplete,
          preventBFCache,
          useActor,
          cleanup
        );
      }
    );
  } else if (cleanup) {
    cleanup();
  }
}

/**
 * Finish doPageNavigation(), by removing event listeners, adding an unload
 * handler if appropriate, and calling the onNavComplete callback.  This
 * function is called after all the expected events for this navigation have
 * occurred.
 */
function doPageNavigation_complete(
  eventsToListenFor,
  onNavComplete,
  preventBFCache,
  useActor,
  cleanup
) {
  if (useActor) {
    if (preventBFCache) {
      let actor =
        TestWindow.getBrowser().browsingContext.currentWindowGlobal.getActor(
          "DocShellHelpers"
        );
      actor.sendAsyncMessage("docshell_helpers:preventBFCache");
    }
  } else {
    // Unregister our event listeners.
    dump("TEST: removing event listeners\n");
    for (let eventType of eventsToListenFor) {
      TestWindow.getBrowser().removeEventListener(
        eventType,
        pageEventListener,
        true
      );
    }

    // If the .preventBFCache property was set, add an RTCPeerConnection to
    // prevent the page from being bfcached.
    if (preventBFCache) {
      let win = TestWindow.getWindow();
      win.blockBFCache = new win.RTCPeerConnection();
    }
  }

  if (cleanup) {
    cleanup();
  }

  let uri = TestWindow.getBrowser().currentURI.spec;
  if (preventBFCache) {
    // Save the current uri in an array of uri's which shouldn't be
    // stored in the bfcache, for later verification.
    if (!(uri in gUrisNotInBFCache)) {
      gUrisNotInBFCache.push(uri);
    }
  } else if (gNavType == NAV_URI) {
    // If we're navigating to a uri and .preventBFCache was not
    // specified, splice it out of gUrisNotInBFCache if it's there.
    gUrisNotInBFCache.forEach(function (element, index, array) {
      if (element == uri) {
        array.splice(index, 1);
      }
    }, this);
  }

  // Notify the callback now that we're done.
  onNavComplete.call();
}

function promisePageNavigation(params) {
  if (params.hasOwnProperty("onNavComplete")) {
    throw new Error(
      "Can't use a onNavComplete completion callback with promisePageNavigation."
    );
  }
  return new Promise(resolve => {
    params.onNavComplete = resolve;
    doPageNavigation(params);
  });
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

function promisePageEvents(params) {
  if (params.hasOwnProperty("onNavComplete")) {
    throw new Error(
      "Can't use a onNavComplete completion callback with promisePageEvents."
    );
  }
  return new Promise(resolve => {
    params.waitForEventsOnly = true;
    params.onNavComplete = resolve;
    doPageNavigation(params);
  });
}

/**
 * The event listener which listens for expectedEvents.
 */
function pageEventListener(
  event,
  originalTargetIsHTMLDocument = HTMLDocument.isInstance(event.originalTarget)
) {
  try {
    dump(
      "TEST: eventListener received a " +
        event.type +
        " event for page " +
        event.originalTarget.title +
        ", persisted=" +
        event.persisted +
        "\n"
    );
  } catch (e) {
    // Ignore any exception.
  }

  // If this page shouldn't be in the bfcache because it was previously
  // loaded with .preventBFCache, make sure that its pageshow event
  // has .persisted = false, even if the test doesn't explicitly test
  // for .persisted.
  if (
    event.type == "pageshow" &&
    (gNavType == NAV_BACK ||
      gNavType == NAV_FORWARD ||
      gNavType == NAV_GOTOINDEX)
  ) {
    let uri = TestWindow.getBrowser().currentURI.spec;
    if (uri in gUrisNotInBFCache) {
      ok(
        !event.persisted,
        "pageshow event has .persisted = false, even " +
          "though it was loaded with .preventBFCache previously\n"
      );
    }
  }

  if (typeof gUnexpectedEvents != "undefined") {
    is(
      gUnexpectedEvents.indexOf(event.type),
      -1,
      "Should not get unexpected event " + event.type
    );
  }

  // If no expected events were specified, mark the final event as having been
  // triggered when a pageshow event is fired; this will allow
  // doPageNavigation() to return.
  if (typeof gExpectedEvents == "undefined" && event.type == "pageshow") {
    waitForNextPaint(function () {
      gFinalEvent = true;
    });
    return;
  }

  // If there are explicitly no expected events, but we receive one, it's an
  // error.
  if (!gExpectedEvents.length) {
    ok(false, "Unexpected event (" + event.type + ") occurred");
    return;
  }

  // Grab the next expected event, and compare its attributes against the
  // actual event.
  let expected = gExpectedEvents.shift();

  is(
    event.type,
    expected.type,
    "A " +
      expected.type +
      " event was expected, but a " +
      event.type +
      " event occurred"
  );

  if (typeof expected.title != "undefined") {
    ok(
      originalTargetIsHTMLDocument,
      "originalTarget for last " + event.type + " event not an HTMLDocument"
    );
    is(
      event.originalTarget.title,
      expected.title,
      "A " +
        event.type +
        " event was expected for page " +
        expected.title +
        ", but was fired for page " +
        event.originalTarget.title
    );
  }

  if (typeof expected.persisted != "undefined") {
    is(
      event.persisted,
      expected.persisted,
      "The persisted property of the " +
        event.type +
        " event on page " +
        event.originalTarget.location +
        " had an unexpected value"
    );
  }

  if ("visibilityState" in expected) {
    is(
      event.originalTarget.visibilityState,
      expected.visibilityState,
      "The visibilityState property of the document on page " +
        event.originalTarget.location +
        " had an unexpected value"
    );
  }

  if ("hidden" in expected) {
    is(
      event.originalTarget.hidden,
      expected.hidden,
      "The hidden property of the document on page " +
        event.originalTarget.location +
        " had an unexpected value"
    );
  }

  // If we're out of expected events, let doPageNavigation() return.
  if (!gExpectedEvents.length) {
    waitForNextPaint(function () {
      gFinalEvent = true;
    });
  }
}

DocShellHelpersParent.eventListener = pageEventListener;

/**
 * End a test.
 */
function finish() {
  // Work around bug 467960.
  let historyPurged;
  if (SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    let history = TestWindow.getBrowser().browsingContext?.sessionHistory;
    history.purgeHistory(history.count);
    historyPurged = Promise.resolve();
  } else {
    historyPurged = SpecialPowers.spawn(TestWindow.getBrowser(), [], () => {
      let history = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory
        .legacySHistory;
      history.purgeHistory(history.count);
    });
  }

  // If the test changed the value of max_total_viewers via a call to
  // enableBFCache(), then restore it now.
  if (typeof gOrigMaxTotalViewers != "undefined") {
    Services.prefs.setIntPref(
      "browser.sessionhistory.max_total_viewers",
      gOrigMaxTotalViewers
    );
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

  historyPurged.then(_ => {
    window.close();
  });
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
  promiseTrue(fn, timeout).then(() => {
    onWaitComplete.call();
  });
}

function promiseTrue(fn, timeout) {
  if (typeof timeout != "undefined") {
    // If timeoutWait is less than 500, assume it represents seconds, and
    // convert to ms.
    if (timeout < 500) {
      timeout *= 1000;
    }
  }

  // Loop until the test function returns true, or until a timeout occurs,
  // if a timeout is defined.
  let intervalid, timeoutid;
  let condition = new Promise(resolve => {
    intervalid = setInterval(async () => {
      if (await fn.call()) {
        resolve();
      }
    }, 20);
  });
  if (typeof timeout != "undefined") {
    condition = Promise.race([
      condition,
      new Promise((_, reject) => {
        timeoutid = setTimeout(() => {
          reject();
        }, timeout);
      }),
    ]);
  }
  return condition
    .finally(() => {
      clearInterval(intervalid);
    })
    .then(() => {
      clearTimeout(timeoutid);
    });
}

function waitForNextPaint(cb) {
  requestAnimationFrame(_ => requestAnimationFrame(cb));
}

function promiseNextPaint() {
  return new Promise(resolve => {
    waitForNextPaint(resolve);
  });
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
  if (typeof gOrigMaxTotalViewers == "undefined") {
    gOrigMaxTotalViewers = Services.prefs.getIntPref(
      "browser.sessionhistory.max_total_viewers"
    );
  }

  if (typeof enable == "boolean") {
    if (enable) {
      Services.prefs.setIntPref("browser.sessionhistory.max_total_viewers", -1);
    } else {
      Services.prefs.setIntPref("browser.sessionhistory.max_total_viewers", 0);
    }
  } else if (typeof enable == "number") {
    Services.prefs.setIntPref(
      "browser.sessionhistory.max_total_viewers",
      enable
    );
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
TestWindow.getWindow = function () {
  return document.getElementById("content").contentWindow;
};
TestWindow.getBrowser = function () {
  return document.getElementById("content");
};
TestWindow.getDocument = function () {
  return document.getElementById("content").contentDocument;
};
