// A common module to run tests on the AccessFu module

'use strict';

/*global isDeeply, getMainChromeWindow, SimpleTest, SpecialPowers, Logger,
  AccessFu, Utils, addMessageListener, currentTabDocument, currentBrowser*/

/**
  * A global variable holding an array of test functions.
  */
var gTestFuncs = [];
/**
  * A global Iterator for the array of test functions.
  */
var gIterator;

Components.utils.import('resource://gre/modules/Services.jsm');
Components.utils.import("resource://gre/modules/accessibility/Utils.jsm");
Components.utils.import("resource://gre/modules/accessibility/EventManager.jsm");

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
        if (!(aMessage instanceof Components.interfaces.nsIConsoleMessage)) {
          return;
        }
        if (aMessage.message.indexOf(aWaitForMessage) < 0) {
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
      var data = JSON.parse(aData)[1];
      // Ignore non-relevant outputs.
      if (!data) {
        return;
      }
      isDeeply(data.details.actions, aWaitForData, "Data is correct");
      aListener.apply(listener);
    };
    Services.obs.addObserver(listener, 'accessfu-output', false);
    return listener;
  },

  on: function AccessFuTest_on(aWaitForData, aListener) {
    return this._addObserver(aWaitForData, aListener);
  },

  off: function AccessFuTest_off(aListener) {
    Services.obs.removeObserver(aListener, 'accessfu-output');
  },

  once: function AccessFuTest_once(aWaitForData, aListener) {
    return this._addObserver(aWaitForData, function observerAndRemove() {
      Services.obs.removeObserver(this, 'accessfu-output');
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
    SimpleTest.executeSoon(function () {
      AccessFu.detach();
      SimpleTest.finish();
    });
  },

  nextTest: function AccessFuTest_nextTest() {
    var testFunc;
    try {
      // Get the next test function from the iterator. If none left,
      // StopIteration exception is thrown.
      testFunc = gIterator.next()[1];
    } catch (ex) {
      // StopIteration exception.
      this.finish();
      return;
    }
    testFunc();
  },

  runTests: function AccessFuTest_runTests() {
    if (gTestFuncs.length === 0) {
      ok(false, "No tests specified!");
      SimpleTest.finish();
      return;
    }

    // Create an Iterator for gTestFuncs array.
    gIterator = Iterator(gTestFuncs); // jshint ignore:line

    // Start AccessFu and put it in stand-by.
    Components.utils.import("resource://gre/modules/accessibility/AccessFu.jsm");

    AccessFu.attach(getMainChromeWindow(window));

    AccessFu.readyCallback = function readyCallback() {
      // Enable logging to the console service.
      Logger.test = true;
      Logger.logLevel = Logger.DEBUG;
    };

    SpecialPowers.pushPrefEnv({
      'set': [['accessibility.accessfu.notify_output', 1],
              ['dom.mozSettings.enabled', true]]
    }, function () {
      if (AccessFuTest._waitForExplicitFinish) {
        // Run all test functions asynchronously.
        AccessFuTest.nextTest();
      } else {
        // Run all test functions synchronously.
        [testFunc() for (testFunc of gTestFuncs)]; // jshint ignore:line
        AccessFuTest.finish();
      }
    });
  }
};

function AccessFuContentTest(aFuncResultPairs) {
  this.queue = aFuncResultPairs;
}

AccessFuContentTest.prototype = {
  currentPair: null,

  start: function(aFinishedCallback) {
    this.finishedCallback = aFinishedCallback;
    var self = this;

    // Get top content message manager, and set it up.
    this.mms = [Utils.getMessageManager(currentBrowser())];
    this.setupMessageManager(this.mms[0], function () {
      // Get child message managers and set them up
      var frames = currentTabDocument().querySelectorAll('iframe');
      var toSetup = 0;
      for (var i = 0; i < frames.length; i++ ) {
        var mm = Utils.getMessageManager(frames[i]);
        if (mm) {
          toSetup++;
          self.mms.push(mm);
          self.setupMessageManager(mm, function () {
            if (--toSetup === 0) {
              // All message managers are loaded and ready to go.
              self.pump();
            }
          });
        }
      }
    });
  },

  setupMessageManager:  function (aMessageManager, aCallback) {
    function contentScript() {
      addMessageListener('AccessFuTest:Focus', function (aMessage) {
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

    aMessageManager.addMessageListener('AccessFu:Present', this);
    aMessageManager.addMessageListener('AccessFu:Ready', function () {
      aMessageManager.addMessageListener('AccessFu:ContentStarted', aCallback);
      aMessageManager.sendAsyncMessage('AccessFu:Start', { buildApp: 'browser' });
    });

    aMessageManager.loadFrameScript(
      'chrome://global/content/accessibility/content-script.js', false);
    aMessageManager.loadFrameScript(
      'data:,(' + contentScript.toString() + ')();', false);
  },

  pump: function() {
    this.currentPair = this.queue.shift();

    if (this.currentPair) {
      if (this.currentPair[0] instanceof Function) {
        this.currentPair[0](this.mms[0]);
      } else if (this.currentPair[0]) {
        this.mms[0].sendAsyncMessage(this.currentPair[0].name,
         this.currentPair[0].json);
      }

      if (!this.currentPair[1]) {
       this.pump();
     }
    } else if (this.finishedCallback) {
      for (var mm of this.mms) {
        mm.sendAsyncMessage('AccessFu:Stop');
      }
      this.finishedCallback();
    }
  },

  receiveMessage: function(aMessage) {
    if (!this.currentPair) {
      return;
    }

    var expected = this.currentPair[1];

    if (expected) {
      if (expected.speak !== undefined) {
        var speech = this.extractUtterance(aMessage.json);
        if (!speech) {
          // Probably a visual highlight adjustment after a scroll.
          return;
        }
        var checkFunc = SimpleTest[expected.speak_checkFunc] || is;
        checkFunc(speech, expected.speak);
      }
    }

    this.pump();
  },

  extractUtterance: function(aData) {
    for (var output of aData) {
      if (output && output.type === 'Speech') {
        for (var action of output.details.actions) {
          if (action && action.method == 'speak') {
            return action.data;
          }
        }
      }
    }

    return null;
  }
};
