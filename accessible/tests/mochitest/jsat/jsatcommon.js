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
      AccessFu.detach();
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

    let chromeWin = getMainChromeWindow(window);
    chromeWin.WindowEventDispatcher = {
      dispatch: () => {},
      sendRequest: () => {}
    };

    AccessFu.readyCallback = function readyCallback() {
      // Enable logging to the console service.
      Logger.test = true;
      Logger.logLevel = Logger.DEBUG;
    };

    AccessFu.attach(chromeWin, true);

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

function AccessFuContentTest(aFuncResultPairs) {
  this.queue = aFuncResultPairs;
}

AccessFuContentTest.prototype = {
  expected: [],
  currentAction: null,
  actionNum: -1,

  start(aFinishedCallback) {
    Logger.logLevel = Logger.DEBUG;
    this.finishedCallback = aFinishedCallback;
    var self = this;

    // Get top content message manager, and set it up.
    this.mms = [Utils.getMessageManager(currentBrowser())];
    this.setupMessageManager(this.mms[0], function() {
      // Get child message managers and set them up
      var frames = currentTabDocument().querySelectorAll("iframe");
      if (frames.length === 0) {
        self.pump();
        return;
      }

      var toSetup = 0;
      for (var i = 0; i < frames.length; i++ ) {
        var mm = Utils.getMessageManager(frames[i]);
        if (mm) {
          toSetup++;
          self.mms.push(mm);
          self.setupMessageManager(mm, function() {
            if (--toSetup === 0) {
              // All message managers are loaded and ready to go.
              self.pump();
            }
          });
        }
      }
    });
  },

  finish() {
    Logger.logLevel = Logger.INFO;
    for (var mm of this.mms) {
        mm.sendAsyncMessage("AccessFu:Stop");
        mm.removeMessageListener("AccessFu:Present", this);
        mm.removeMessageListener("AccessFu:Input", this);
        mm.removeMessageListener("AccessFu:CursorCleared", this);
        mm.removeMessageListener("AccessFu:Focused", this);
        mm.removeMessageListener("AccessFu:AriaHidden", this);
        mm.removeMessageListener("AccessFu:Ready", this);
        mm.removeMessageListener("AccessFu:ContentStarted", this);
      }
    if (this.finishedCallback) {
      this.finishedCallback();
    }
  },

  setupMessageManager(aMessageManager, aCallback) {
    function contentScript() {
      addMessageListener("AccessFuTest:Focus", function(aMessage) {
        var elem = content.document.querySelector(aMessage.json.selector);
        if (elem) {
          if (aMessage.json.blur) {
            elem.blur();
          } else {
            elem.focus();
          }
        }
      });
    }

    aMessageManager.addMessageListener("AccessFu:Present", this);
    aMessageManager.addMessageListener("AccessFu:Input", this);
    aMessageManager.addMessageListener("AccessFu:CursorCleared", this);
    aMessageManager.addMessageListener("AccessFu:Focused", this);
    aMessageManager.addMessageListener("AccessFu:AriaHidden", this);
    aMessageManager.addMessageListener("AccessFu:Ready", function() {
      aMessageManager.addMessageListener("AccessFu:ContentStarted", aCallback);
      aMessageManager.sendAsyncMessage("AccessFu:Start",
        { buildApp: "browser",
          androidSdkVersion: Utils.AndroidSdkVersion,
          logLevel: "DEBUG",
          inTest: true });
    });

    aMessageManager.loadFrameScript(
      "chrome://global/content/accessibility/content-script.js", false);
    aMessageManager.loadFrameScript(
      "data:,(" + contentScript.toString() + ")();", false);
  },

  pump() {
    this.expected.shift();
    if (this.expected.length) {
      return;
    }

    var currentPair = this.queue.shift();

    if (currentPair) {
      this.actionNum++;
      this.currentAction = currentPair[0];
      if (typeof this.currentAction === "function") {
        this.currentAction(this.mms[0]);
      } else if (this.currentAction) {
        this.mms[0].sendAsyncMessage(this.currentAction.name,
         this.currentAction.json);
      }

      this.expected = currentPair.slice(1, currentPair.length);

      if (!this.expected[0]) {
       this.pump();
     }
    } else {
      this.finish();
    }
  },

  receiveMessage(aMessage) {
    var expected = this.expected[0];

    if (!expected) {
      return;
    }

    var actionsString = typeof this.currentAction === "function" ?
      this.currentAction.toString() : JSON.stringify(this.currentAction);

    if (typeof expected === "string") {
      ok(true, "Got " + expected + " after " + actionsString);
      this.pump();
    } else if (expected.ignore && !expected.ignore(aMessage)) {
      expected.is(aMessage.json, "after " + actionsString +
        " (" + this.actionNum + ")");
      expected.is_correct_focus();
      this.pump();
    }
  }
};

// Common content messages

var ContentMessages = {
  simpleMoveFirst: {
    name: "AccessFu:MoveCursor",
    json: {
      action: "moveFirst",
      rule: "Simple",
      inputType: "gesture",
      origin: "top"
    }
  },

  simpleMoveLast: {
    name: "AccessFu:MoveCursor",
    json: {
      action: "moveLast",
      rule: "Simple",
      inputType: "gesture",
      origin: "top"
    }
  },

  simpleMoveNext: {
    name: "AccessFu:MoveCursor",
    json: {
      action: "moveNext",
      rule: "Simple",
      inputType: "gesture",
      origin: "top"
    }
  },

  simpleMovePrevious: {
    name: "AccessFu:MoveCursor",
    json: {
      action: "movePrevious",
      rule: "Simple",
      inputType: "gesture",
      origin: "top"
    }
  },

  clearCursor: {
    name: "AccessFu:ClearCursor",
    json: {
      origin: "top"
    }
  },

  moveOrAdjustUp: function moveOrAdjustUp(aRule) {
    return {
      name: "AccessFu:MoveCursor",
      json: {
        origin: "top",
        action: "movePrevious",
        inputType: "gesture",
        rule: (aRule || "Simple"),
        adjustRange: true
      }
    };
  },

  moveOrAdjustDown: function moveOrAdjustUp(aRule) {
    return {
      name: "AccessFu:MoveCursor",
      json: {
        origin: "top",
        action: "moveNext",
        inputType: "gesture",
        rule: (aRule || "Simple"),
        adjustRange: true
      }
    };
  },

  androidScrollForward: function adjustUp() {
    return {
      name: "AccessFu:AndroidScroll",
      json: { origin: "top", direction: "forward" }
    };
  },

  androidScrollBackward: function adjustDown() {
    return {
      name: "AccessFu:AndroidScroll",
      json: { origin: "top", direction: "backward" }
    };
  },

  focusSelector: function focusSelector(aSelector, aBlur) {
    return {
      name: "AccessFuTest:Focus",
      json: {
        selector: aSelector,
        blur: aBlur
      }
    };
  },

  activateCurrent: function activateCurrent(aOffset) {
    return {
      name: "AccessFu:Activate",
      json: {
        origin: "top",
        offset: aOffset
      }
    };
  },

  moveNextBy: function moveNextBy(aGranularity) {
    return {
      name: "AccessFu:MoveByGranularity",
      json: {
        direction: "Next",
        granularity: this._granularityMap[aGranularity]
      }
    };
  },

  movePreviousBy: function movePreviousBy(aGranularity) {
    return {
      name: "AccessFu:MoveByGranularity",
      json: {
        direction: "Previous",
        granularity: this._granularityMap[aGranularity]
      }
    };
  },

  moveCaretNextBy: function moveCaretNextBy(aGranularity) {
    return {
      name: "AccessFu:MoveCaret",
      json: {
        direction: "Next",
        granularity: this._granularityMap[aGranularity]
      }
    };
  },

  moveCaretPreviousBy: function moveCaretPreviousBy(aGranularity) {
    return {
      name: "AccessFu:MoveCaret",
      json: {
        direction: "Previous",
        granularity: this._granularityMap[aGranularity]
      }
    };
  },

  _granularityMap: {
    "character": 1, // MOVEMENT_GRANULARITY_CHARACTER
    "word": 2, // MOVEMENT_GRANULARITY_WORD
    "paragraph": 8 // MOVEMENT_GRANULARITY_PARAGRAPH
  }
};

function ExpectedMessage(aName, aOptions) {
  this.name = aName;
  this.options = aOptions || {};
  this.json = {};
}

ExpectedMessage.prototype.lazyCompare = function(aReceived, aExpected, aInfo) {
  if (aExpected && !aReceived) {
    return [false, "Expected something but got nothing -- " + aInfo];
  }

  if (typeof aReceived === "string" || typeof aExpected === "string") {
    return [aReceived == aExpected, `String comparison: Got '${aReceived}.', expected: ${aExpected} -- ${aInfo}`];
  }

  var matches = true;
  var delta = [];
  for (var attr in aExpected) {
    var expected = aExpected[attr];
    var received = aReceived[attr];
    if (typeof expected === "object") {
      var [childMatches, childDelta] = this.lazyCompare(received, expected);
      if (!childMatches) {
        delta.push(attr + " [ " + childDelta + " ]");
        matches = false;
      }
    } else if (received !== expected) {
      delta.push(
        attr + " [ expected " + JSON.stringify(expected) +
        " got " + JSON.stringify(received) + " ]");
      matches = false;
    }
  }

  var msg = delta.length ? delta.join(" ") : "Structures lazily match";
  return [matches, msg + " -- " + aInfo];
};

ExpectedMessage.prototype.is = function(aReceived, aInfo) {
  var checkFunc = this.options.todo ? "todo" : "ok";
  SimpleTest[checkFunc].apply(
    SimpleTest, this.lazyCompare(aReceived, this.json, aInfo));
};

ExpectedMessage.prototype.is_correct_focus = function(aInfo) {
  if (!this.options.focused) {
    return;
  }

  var checkFunc = this.options.focused_todo ? "todo_is" : "is";
  var doc = currentTabDocument();
  SimpleTest[checkFunc].apply(SimpleTest,
    [ doc.activeElement, doc.querySelector(this.options.focused),
      "Correct element is focused: " + this.options.focused + " -- " + aInfo ]);
};

ExpectedMessage.prototype.ignore = function(aMessage) {
  return aMessage.name !== this.name;
};

function ExpectedPresent(aAndroidEvents, aOptions) {
  ExpectedMessage.call(this, "AccessFu:Present", aOptions);
  this.expectedEvents = aAndroidEvents;
}

ExpectedPresent.prototype = Object.create(ExpectedMessage.prototype);

ExpectedPresent.prototype.is = function(aReceived, aInfo) {
  if (typeof this.expectedEvents == "string") {
    // This is an event we have yet to implement, do a simple string comparison.
    if (this.expectedEvents == aReceived) {
      SimpleTest.todo(false, `${aInfo} (${aReceived})`);
      return;
    }
  }

  SimpleTest[this.options.todo ? "todo" : "ok"].apply(SimpleTest,
    this.lazyCompare(aReceived, this.expectedEvents, aInfo + " aReceived: " +
      JSON.stringify(aReceived) + " evt: " + JSON.stringify(this.expectedEvents)));
};

ExpectedPresent.prototype.ignore = function(aMessage) {
  if (!aMessage.json || ExpectedMessage.prototype.ignore.call(this, aMessage)) {
    return true;
  }

  let firstEvent = (aMessage.json || [])[0];

  return firstEvent && firstEvent.eventType === AndroidEvents.ANDROID_VIEW_SCROLLED;
};

function ExpectedCursorChange(aSpeech, aOptions) {
  ExpectedPresent.call(this, [{
    eventType: 0x8000, // VIEW_ACCESSIBILITY_FOCUSED
  }], aOptions);
}

ExpectedCursorChange.prototype = Object.create(ExpectedPresent.prototype);

function ExpectedCursorTextChange(aSpeech, aStartOffset, aEndOffset, aOptions) {
  ExpectedPresent.call(this, [{
    eventType: AndroidEvents.ANDROID_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY,
    fromIndex: aStartOffset,
    toIndex: aEndOffset
  }], aOptions);

  // bug 980509
  this.options.b2g_todo = true;
}

ExpectedCursorTextChange.prototype =
  Object.create(ExpectedCursorChange.prototype);

function ExpectedClickAction(aOptions) {
  ExpectedPresent.call(this, [{
    eventType: AndroidEvents.ANDROID_VIEW_CLICKED
  }], aOptions);
}

ExpectedClickAction.prototype = Object.create(ExpectedPresent.prototype);

function ExpectedCheckAction(aChecked, aOptions) {
  ExpectedPresent.call(this, [{
    eventType: AndroidEvents.ANDROID_VIEW_CLICKED,
    checked: aChecked
  }], aOptions);
}

ExpectedCheckAction.prototype = Object.create(ExpectedPresent.prototype);

function ExpectedSwitchAction(aSwitched, aOptions) {
  ExpectedPresent.call(this, [{
    eventType: AndroidEvents.ANDROID_VIEW_CLICKED,
    checked: aSwitched
  }], aOptions);
}

ExpectedSwitchAction.prototype = Object.create(ExpectedPresent.prototype);

// XXX: Implement Android event?
function ExpectedNameChange(aName, aOptions) {
  ExpectedPresent.call(this, "todo.name-changed", aOptions);
}

ExpectedNameChange.prototype = Object.create(ExpectedPresent.prototype);

// XXX: Implement Android event?
function ExpectedValueChange(aValue, aOptions) {
  ExpectedPresent.call(this, "todo.value-changed", aOptions);
}

ExpectedValueChange.prototype = Object.create(ExpectedPresent.prototype);

// XXX: Implement Android event?
function ExpectedTextChanged(aValue, aOptions) {
  ExpectedPresent.call(this, [{
    eventType: AndroidEvents.ANDROID_VIEW_TEXT_CHANGED
  }], aOptions);
}

ExpectedTextChanged.prototype = Object.create(ExpectedPresent.prototype);

function ExpectedEditState(aEditState, aOptions) {
  ExpectedMessage.call(this, "AccessFu:Input", aOptions);
  this.json = aEditState;
}

ExpectedEditState.prototype = Object.create(ExpectedMessage.prototype);

function ExpectedTextSelectionChanged(aStart, aEnd, aOptions) {
  ExpectedPresent.call(this, [{
    eventType: AndroidEvents.ANDROID_VIEW_TEXT_SELECTION_CHANGED,
  }], aOptions);
}

ExpectedTextSelectionChanged.prototype =
  Object.create(ExpectedPresent.prototype);

function ExpectedTextCaretChanged(aFrom, aTo, aOptions) {
  ExpectedPresent.call(this, [{
    eventType: AndroidEvents.ANDROID_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY,
    fromIndex: aFrom,
    toIndex: aTo
  }], aOptions);
}

ExpectedTextCaretChanged.prototype = Object.create(ExpectedPresent.prototype);

function ExpectedAnnouncement(aAnnouncement, aOptions) {
  ExpectedPresent.call(this, [{
    eventType: AndroidEvents.ANDROID_ANNOUNCEMENT,
    text: [ aAnnouncement],
    addedCount: aAnnouncement.length
  }], aOptions);
}

ExpectedAnnouncement.prototype = Object.create(ExpectedPresent.prototype);

// XXX: Implement Android event?
function ExpectedNoMove(aOptions) {
  ExpectedPresent.call(this, null, aOptions);
}

ExpectedNoMove.prototype = Object.create(ExpectedPresent.prototype);
