// A common module to run tests on the AccessFu module

"use strict";

/* global isDeeply, getMainChromeWindow, SimpleTest, SpecialPowers, Logger,
  AccessFu, Utils, addMessageListener, currentTabDocument, currentBrowser*/

/**
  * A global variable holding an array of test functions.
  */
var gTestFuncs = [];
/**
  * A global Iterator for the array of test functions.
  */
var gIterator;

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.import("resource://gre/modules/accessibility/EventManager.jsm");
ChromeUtils.import("resource://gre/modules/accessibility/Constants.jsm");

const MovementGranularity = {
  CHARACTER: 1,
  WORD: 2,
  LINE: 4,
  PARAGRAPH: 8
};

var AccessFuTest = {

  addFunc: function AccessFuTest_addFunc(aFunc) {
    if (aFunc) {
      gTestFuncs.push(aFunc);
    }
  },

  _registerListener: function AccessFuTest__registerListener(aWaitForMessage, aListenerFunc) {
    var listener = {
      observe: function observe(aMessage) {
        // Ignore unexpected messages.
        if (!(aMessage instanceof Ci.nsIConsoleMessage)) {
          return;
        }
        if (!aMessage.message.includes(aWaitForMessage)) {
          return;
        }
        aListenerFunc.apply(listener);
      }
    };
    Services.console.registerListener(listener);
    return listener;
  },

  on_log: function AccessFuTest_on_log(aWaitForMessage, aListenerFunc) {
    return this._registerListener(aWaitForMessage, aListenerFunc);
  },

  off_log: function AccessFuTest_off_log(aListener) {
    Services.console.unregisterListener(aListener);
  },

  once_log: function AccessFuTest_once_log(aWaitForMessage, aListenerFunc) {
    return this._registerListener(aWaitForMessage,
      function listenAndUnregister() {
        Services.console.unregisterListener(this);
        aListenerFunc();
      });
  },

  _addObserver: function AccessFuTest__addObserver(aWaitForData, aListener) {
    var listener = function listener(aSubject, aTopic, aData) {
      var data = JSON.parse(aData);
      // Ignore non-relevant outputs.
      if (!data || (data[0] && data[0].text && data[0].text[0] == "new tab")) {
        return;
      }

      isDeeply(data, aWaitForData, "Data is correct (" + aData + ")");
      aListener.apply(listener);
    };
    Services.obs.addObserver(listener, "accessibility-output");
    return listener;
  },

  on: function AccessFuTest_on(aWaitForData, aListener) {
    return this._addObserver(aWaitForData, aListener);
  },

  off: function AccessFuTest_off(aListener) {
    Services.obs.removeObserver(aListener, "accessibility-output");
  },

  once: function AccessFuTest_once(aWaitForData, aListener) {
    return this._addObserver(aWaitForData, function observerAndRemove() {
      Services.obs.removeObserver(this, "accessibility-output");
      aListener();
    });
  },

  _waitForExplicitFinish: false,

  waitForExplicitFinish: function AccessFuTest_waitForExplicitFinish() {
    this._waitForExplicitFinish = true;
  },

  finish: function AccessFuTest_finish() {
    // Disable the console service logging.
    Logger.test = false;
    Logger.logLevel = Logger.INFO;
    // Finish through idle callback to let AccessFu._disable complete.
    SimpleTest.executeSoon(function() {
      // May be redundant, but for cleanup's sake.
      AccessFu.disable();
      SimpleTest.finish();
    });
  },

  nextTest: function AccessFuTest_nextTest() {
    var result = gIterator.next();
    if (result.done) {
      this.finish();
      return;
    }
    var testFunc = result.value;
    testFunc();
  },

  runTests: function AccessFuTest_runTests(aAdditionalPrefs) {
    if (gTestFuncs.length === 0) {
      ok(false, "No tests specified!");
      SimpleTest.finish();
      return;
    }

    // Create an Iterator for gTestFuncs array.
    gIterator = (function* () {
      for (var testFunc of gTestFuncs) {
        yield testFunc;
      }
    })();

    // Start AccessFu and put it in stand-by.
    ChromeUtils.import("resource://gre/modules/accessibility/AccessFu.jsm");

    AccessFu.readyCallback = function readyCallback() {
      // Enable logging to the console service.
      Logger.test = true;
      Logger.logLevel = Logger.DEBUG;
    };

    var prefs = [["accessibility.accessfu.notify_output", 1]];
    prefs.push.apply(prefs, aAdditionalPrefs);

    SpecialPowers.pushPrefEnv({ "set": prefs }, function() {
      if (AccessFuTest._waitForExplicitFinish) {
        // Run all test functions asynchronously.
        AccessFuTest.nextTest();
      } else {
        // Run all test functions synchronously.
        gTestFuncs.forEach(testFunc => testFunc());
        AccessFuTest.finish();
      }
    });
  }
};

class AccessFuContentTestRunner {
  constructor() {
    this.listenersMap = new Map();
    let frames = Array.from(currentTabDocument().querySelectorAll("iframe"));
    this.mms = [Utils.getMessageManager(currentBrowser()),
      ...frames.map(f => Utils.getMessageManager(f)).filter(mm => !!mm)];
  }

  start(aFinishedCallback) {
    Logger.logLevel = Logger.DEBUG;
    this.finishedCallback = aFinishedCallback;

    return Promise.all(this.mms.map(mm => this.setupMessageManager(mm)));
  }

  finish() {
    Logger.logLevel = Logger.INFO;
    this.listenersMap.clear();
    for (var mm of this.mms) {
      mm.sendAsyncMessage("AccessFu:Stop");
      mm.removeMessageListener("AccessFu:Present", this);
    }
  }

  sendMessage(message) {
    // First message manager is the top-level one.
    this.mms[0].sendAsyncMessage(message.name, message.data);
  }

  androidEvent(eventType) {
    return new Promise(resolve => {
      if (!this.listenersMap.has(eventType)) {
        this.listenersMap.set(eventType, [resolve]);
      } else {
        this.listenersMap.get(eventType).push(resolve);
      }
    }).then(evt => {
      if (this.debug) {
        info("Resolving event: " + evt.eventType);
      }
      return evt;
    });
  }

  isFocused(aExpected) {
    var doc = currentTabDocument();
    SimpleTest.is(doc.activeElement, doc.querySelector(aExpected),
      "Correct element is focused: " + aExpected);
  }

  async setupMessageManager(aMessageManager) {
    function contentScript() {
      addMessageListener("AccessFuTest:Focus", aMessage => {
        var elem = content.document.querySelector(aMessage.data.selector);
        if (elem) {
          elem.focus();
        }
      });

      addMessageListener("AccessFuTest:Blur", () => {
        content.document.activeElement.blur();
      });
    }

    aMessageManager.loadFrameScript(
      "data:,(" + contentScript.toString() + ")();", false);

    let readyPromise = new Promise(resolve =>
      aMessageManager.addMessageListener("AccessFu:Ready", resolve));

    aMessageManager.loadFrameScript(
      "chrome://global/content/accessibility/content-script.js", false);

    await readyPromise;

    let startedPromise = new Promise(resolve =>
      aMessageManager.addMessageListener("AccessFu:ContentStarted", resolve));

    aMessageManager.sendAsyncMessage("AccessFu:Start",
      { buildApp: "browser",
        androidSdkVersion: Utils.AndroidSdkVersion,
        logLevel: "DEBUG",
        inTest: true });

    await startedPromise;

    aMessageManager.addMessageListener("AccessFu:Present", this);
  }

  receiveMessage(aMessage) {
    if (aMessage.name != "AccessFu:Present" || !aMessage.data) {
      return;
    }

    for (let evt of aMessage.data) {
      if (this.debug) {
        info("Android event: " + JSON.stringify(evt));
      }
      let listener = (this.listenersMap.get(evt.eventType || evt) || []).shift();
      if (listener) {
        listener(evt);
      }
    }
  }

  async expectAndroidEvents(aFunc, ...aExpectedEvents) {
    let events = Promise.all(aExpectedEvents.map(e => this.androidEvent(e)));
    aFunc();
    let gotEvents = await events;
    return gotEvents.length == 1 ? gotEvents[0] : gotEvents;
  }

  moveCursor(aArgs, ...aExpectedEvents) {
    return this.expectAndroidEvents(() => {
      this.sendMessage({
        name: "AccessFu:MoveCursor",
        data: {
          inputType: "gesture",
          origin: "top",
          ...aArgs
        }
      });
    }, ...aExpectedEvents);
  }

  moveNext(aRule, ...aExpectedEvents) {
    return this.moveCursor({ action: "moveNext", rule: aRule },
      ...aExpectedEvents);
  }

  movePrevious(aRule, ...aExpectedEvents) {
    return this.moveCursor({ action: "movePrevious", rule: aRule },
      ...aExpectedEvents);
  }

  moveFirst(aRule, ...aExpectedEvents) {
    return this.moveCursor({ action: "moveFirst", rule: aRule },
      ...aExpectedEvents);
  }

  moveLast(aRule, ...aExpectedEvents) {
    return this.moveCursor({ action: "moveLast", rule: aRule },
      ...aExpectedEvents);
  }

  async clearCursor() {
    return new Promise(resolve => {
      let _listener = (msg) => {
        this.mms.forEach(
          mm => mm.removeMessageListener("AccessFu:CursorCleared", _listener));
        resolve();
      };
      this.mms.forEach(
        mm => mm.addMessageListener("AccessFu:CursorCleared", _listener));
      this.sendMessage({
        name: "AccessFu:ClearCursor",
        data: {
          origin: "top"
        }
      });
    });
  }

  focusSelector(aSelector, ...aExpectedEvents) {
    return this.expectAndroidEvents(() => {
      this.sendMessage({
        name: "AccessFuTest:Focus",
        data: {
          selector: aSelector
        }
      });
    }, ...aExpectedEvents);
  }

  blur(...aExpectedEvents) {
    return this.expectAndroidEvents(() => {
      this.sendMessage({ name: "AccessFuTest:Blur" });
    }, ...aExpectedEvents);
  }

  activateCurrent(aOffset, ...aExpectedEvents) {
    return this.expectAndroidEvents(() => {
      this.sendMessage({
        name: "AccessFu:Activate",
        data: {
          origin: "top",
          offset: aOffset
        }
      });
    }, ...aExpectedEvents);
  }

  typeKey(aKey, ...aExpectedEvents) {
    return this.expectAndroidEvents(() => {
      synthesizeKey(aKey, {}, currentTabWindow());
    }, ...aExpectedEvents);
  }

  eventTextMatches(aEvent, aExpected) {
    isDeeply(aEvent.text, aExpected,
      "Event text matches. " +
      `Got ${JSON.stringify(aEvent.text)}, expected ${JSON.stringify(aExpected)}.`);
  }

  androidScrollForward() {
    this.sendMessage({
      name: "AccessFu:AndroidScroll",
      data: { origin: "top", direction: "forward" }
    });
  }

  androidScrollBackward() {
    this.sendMessage({
      name: "AccessFu:AndroidScroll",
      data: { origin: "top", direction: "backward" }
    });
  }

  moveByGranularity(aDirection, aGranularity, ...aExpectedEvents) {
    return this.expectAndroidEvents(() => {
      this.sendMessage({
        name: "AccessFu:MoveByGranularity",
        data: {
          direction: aDirection,
          granularity: aGranularity
        }
      });
    }, ...aExpectedEvents);
  }

  moveNextByGranularity(aGranularity, ...aExpectedEvents) {
    return this.moveByGranularity("Next", aGranularity, ...aExpectedEvents);
  }

  movePreviousByGranularity(aGranularity, ...aExpectedEvents) {
    return this.moveByGranularity("Previous", aGranularity, ...aExpectedEvents);
  }
}
