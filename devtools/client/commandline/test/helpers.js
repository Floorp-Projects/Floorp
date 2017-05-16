/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

// A copy of this code exists in firefox mochitests. They should be kept
// in sync. Hence the exports synonym for non AMD contexts.
var { helpers, assert } = (function () {

  var helpers = {};

  var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
  var { TargetFactory } = require("devtools/client/framework/target");
  var Services = require("Services");

  var assert = { ok: ok, is: is, log: info };
  var util = require("gcli/util/util");
  var cli = require("gcli/cli");
  var KeyEvent = require("gcli/util/util").KeyEvent;

  const { GcliFront } = require("devtools/shared/fronts/gcli");

/**
 * See notes in helpers.checkOptions()
 */
  var createDeveloperToolbarAutomator = function (toolbar) {
    var automator = {
      setInput: function (typed) {
        return toolbar.inputter.setInput(typed);
      },

      setCursor: function (cursor) {
        return toolbar.inputter.setCursor(cursor);
      },

      focus: function () {
        return toolbar.inputter.focus();
      },

      fakeKey: function (keyCode) {
        var fakeEvent = {
          keyCode: keyCode,
          preventDefault: function () { },
          timeStamp: new Date().getTime()
        };

        toolbar.inputter.onKeyDown(fakeEvent);

        if (keyCode === KeyEvent.DOM_VK_BACK_SPACE) {
          var input = toolbar.inputter.element;
          input.value = input.value.slice(0, -1);
        }

        return toolbar.inputter.handleKeyUp(fakeEvent);
      },

      getInputState: function () {
        return toolbar.inputter.getInputState();
      },

      getCompleterTemplateData: function () {
        return toolbar.completer._getCompleterTemplateData();
      },

      getErrorMessage: function () {
        return toolbar.tooltip.errorEle.textContent;
      }
    };

    Object.defineProperty(automator, "focusManager", {
      get: function () { return toolbar.focusManager; },
      enumerable: true
    });

    Object.defineProperty(automator, "field", {
      get: function () { return toolbar.tooltip.field; },
      enumerable: true
    });

    return automator;
  };

/**
 * Warning: For use with Firefox Mochitests only.
 *
 * Open a new tab at a URL and call a callback on load, and then tidy up when
 * the callback finishes.
 * The function will be passed a set of test options, and will usually return a
 * promise to indicate that the tab can be cleared up. (To be formal, we call
 * Promise.resolve() on the return value of the callback function)
 *
 * The options used by addTab include:
 * - chromeWindow: XUL window parent of created tab. a.k.a 'window' in mochitest
 * - tab: The new XUL tab element, as returned by BrowserTestUtils.addTab(gBrowser)
 * - target: The debug target as defined by the devtools framework
 * - browser: The XUL browser element for the given tab
 * - isFirefox: Always true. Allows test sharing with GCLI
 *
 * Normally addTab will create an options object containing the values as
 * described above. However these options can be customized by the third
 * 'options' parameter. This has the ability to customize the value of
 * chromeWindow or isFirefox, and to add new properties.
 *
 * @param url The URL for the new tab
 * @param callback The function to call on page load
 * @param options An optional set of options to customize the way the tests run
 */
  helpers.addTab = function (url, callback, options) {
    waitForExplicitFinish();

    options = options || {};
    options.chromeWindow = options.chromeWindow || window;
    options.isFirefox = true;

    var tabbrowser = options.chromeWindow.gBrowser;
    options.tab = tabbrowser.addTab();
    tabbrowser.selectedTab = options.tab;
    options.browser = tabbrowser.getBrowserForTab(options.tab);
    options.target = TargetFactory.forTab(options.tab);

    var loaded = helpers.listenOnce(options.browser, "load", true).then(function (ev) {
      var reply = callback.call(null, options);

      return Promise.resolve(reply).then(null, function (error) {
        ok(false, error);
      }).then(function () {
        tabbrowser.removeTab(options.tab);

        delete options.target;
        delete options.browser;
        delete options.tab;

        delete options.chromeWindow;
        delete options.isFirefox;
      });
    });

    options.browser.contentWindow.location = url;
    return loaded;
  };

/**
 * Open a new tab
 * @param url Address of the page to open
 * @param options Object to which we add properties describing the new tab. The
 * following properties are added:
 * - chromeWindow
 * - tab
 * - browser
 * - target
 * @return A promise which resolves to the options object when the 'load' event
 * happens on the new tab
 */
  helpers.openTab = function (url, options) {
    waitForExplicitFinish();

    options = options || {};
    options.chromeWindow = options.chromeWindow || window;
    options.isFirefox = true;

    var tabbrowser = options.chromeWindow.gBrowser;
    options.tab = tabbrowser.addTab();
    tabbrowser.selectedTab = options.tab;
    options.browser = tabbrowser.getBrowserForTab(options.tab);
    options.target = TargetFactory.forTab(options.tab);

    return helpers.navigate(url, options);
  };

/**
 * Undo the effects of |helpers.openTab|
 * @param options The options object passed to |helpers.openTab|
 * @return A promise resolved (with undefined) when the tab is closed
 */
  helpers.closeTab = function (options) {
    options.chromeWindow.gBrowser.removeTab(options.tab);

    delete options.target;
    delete options.browser;
    delete options.tab;

    delete options.chromeWindow;
    delete options.isFirefox;

    return Promise.resolve(undefined);
  };

/**
 * Open the developer toolbar in a tab
 * @param options Object to which we add properties describing the developer
 * toolbar. The following properties are added:
 * - automator
 * - requisition
 * @return A promise which resolves to the options object when the 'load' event
 * happens on the new tab
 */
  helpers.openToolbar = function (options) {
    options = options || {};
    options.chromeWindow = options.chromeWindow || window;

    return options.chromeWindow.DeveloperToolbar.show(true).then(function () {
      var toolbar = options.chromeWindow.DeveloperToolbar;
      options.automator = createDeveloperToolbarAutomator(toolbar);
      options.requisition = toolbar.requisition;
      return options;
    });
  };

/**
 * Navigate the current tab to a URL
 */
  helpers.navigate = Task.async(function* (url, options) {
    options = options || {};
    options.chromeWindow = options.chromeWindow || window;
    options.tab = options.tab || options.chromeWindow.gBrowser.selectedTab;

    var tabbrowser = options.chromeWindow.gBrowser;
    options.browser = tabbrowser.getBrowserForTab(options.tab);

    let onLoaded = BrowserTestUtils.browserLoaded(options.browser);
    options.browser.loadURI(url);
    yield onLoaded;

    return options;
  });

/**
 * Undo the effects of |helpers.openToolbar|
 * @param options The options object passed to |helpers.openToolbar|
 * @return A promise resolved (with undefined) when the toolbar is closed
 */
  helpers.closeToolbar = function (options) {
    return options.chromeWindow.DeveloperToolbar.hide().then(function () {
      delete options.automator;
      delete options.requisition;
    });
  };

/**
 * A helper to work with Task.spawn so you can do:
 *   return Task.spawn(realTestFunc).then(finish, helpers.handleError);
 */
  helpers.handleError = function (ex) {
    console.error(ex);
    ok(false, ex);
    finish();
  };

/**
 * A helper for calling addEventListener and then removeEventListener as soon
 * as the event is called, passing the results on as a promise
 * @param element The DOM element to listen on
 * @param event The name of the event to listen for
 * @param useCapture Should we use the capturing phase?
 * @return A promise resolved with the event object when the event first happens
 */
  helpers.listenOnce = function (element, event, useCapture) {
    return new Promise((resolve, reject) => {
      var onEvent = function (ev) {
        element.removeEventListener(event, onEvent, useCapture);
        resolve(ev);
      };
      element.addEventListener(event, onEvent, useCapture);
    });
  };

/**
 * A wrapper for calling Services.obs.[add|remove]Observer using promises.
 * @param topic The topic parameter to Services.obs.addObserver
 * @param ownsWeak The ownsWeak parameter to Services.obs.addObserver with a
 * default value of false
 * @return a promise that resolves when the ObserverService first notifies us
 * of the topic. The value of the promise is the first parameter to the observer
 * function other parameters are dropped.
 */
  helpers.observeOnce = function (topic, ownsWeak = false) {
    return new Promise((resolve, reject) => {
      let resolver = function (subject) {
        Services.obs.removeObserver(resolver, topic);
        resolve(subject);
      };
      Services.obs.addObserver(resolver, topic, ownsWeak);
    });
  };

/**
 * Takes a function that uses a callback as its last parameter, and returns a
 * new function that returns a promise instead
 */
  helpers.promiseify = function (functionWithLastParamCallback, scope) {
    return function () {
      let args = [].slice.call(arguments);
      return new Promise(resolve => {
        args.push((...results) => {
          resolve(results.length > 1 ? results : results[0]);
        });
        functionWithLastParamCallback.apply(scope, args);
      });
    };
  };

/**
 * Warning: For use with Firefox Mochitests only.
 *
 * As addTab, but that also opens the developer toolbar. In addition a new
 * 'automator' property is added to the options object which uses the
 * developer toolbar
 */
  helpers.addTabWithToolbar = function (url, callback, options) {
    return helpers.addTab(url, function (innerOptions) {
      var win = innerOptions.chromeWindow;

      return win.DeveloperToolbar.show(true).then(function () {
        var toolbar = win.DeveloperToolbar;
        innerOptions.automator = createDeveloperToolbarAutomator(toolbar);
        innerOptions.requisition = toolbar.requisition;

        var reply = callback.call(null, innerOptions);

        return Promise.resolve(reply).then(null, function (error) {
          ok(false, error);
          console.error(error);
        }).then(function () {
          win.DeveloperToolbar.hide().then(function () {
            delete innerOptions.automator;
          });
        });
      });
    }, options);
  };

/**
 * Warning: For use with Firefox Mochitests only.
 *
 * Run a set of test functions stored in the values of the 'exports' object
 * functions stored under setup/shutdown will be run at the start/end of the
 * sequence of tests.
 * A test will be considered finished when its return value is resolved.
 * @param options An object to be passed to the test functions
 * @param tests An object containing named test functions
 * @return a promise which will be resolved when all tests have been run and
 * their return values resolved
 */
  helpers.runTests = function (options, tests) {
    var testNames = Object.keys(tests).filter(function (test) {
      return test != "setup" && test != "shutdown";
    });

    var recover = function (error) {
      ok(false, error);
      console.error(error, error.stack);
    };

    info("SETUP");
    var setupDone = (tests.setup != null) ?
      Promise.resolve(tests.setup(options)) :
      Promise.resolve();

    var testDone = setupDone.then(function () {
      return util.promiseEach(testNames, function (testName) {
        info(testName);
        var action = tests[testName];

        if (typeof action === "function") {
          var reply = action.call(tests, options);
          return Promise.resolve(reply);
        }
        else if (Array.isArray(action)) {
          return helpers.audit(options, action);
        }

        return Promise.reject("test action '" + testName +
                            "' is not a function or helpers.audit() object");
      });
    }, recover);

    return testDone.then(function () {
      info("SHUTDOWN");
      return (tests.shutdown != null) ?
        Promise.resolve(tests.shutdown(options)) :
        Promise.resolve();
    }, recover);
  };

  const MOCK_COMMANDS_URI = "chrome://mochitests/content/browser/devtools/client/commandline/test/mockCommands.js";

  const defer = function () {
    const deferred = { };
    deferred.promise = new Promise(function (resolve, reject) {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });
    return deferred;
  };

/**
 * This does several actions associated with running a GCLI test in mochitest
 * 1. Create a new tab containing basic markup for GCLI tests
 * 2. Open the developer toolbar
 * 3. Register the mock commands with the server process
 * 4. Wait for the proxy commands to be auto-regitstered with the client
 * 5. Register the mock converters with the client process
 * 6. Run all the tests
 * 7. Tear down all the setup
 */
  helpers.runTestModule = function (exports, name) {
    return Task.spawn(function* () {
      const uri = "data:text/html;charset=utf-8," +
                "<style>div{color:red;}</style>" +
                "<div id='gcli-root'>" + name + "</div>";

      const options = yield helpers.openTab(uri);
      options.isRemote = true;

      yield helpers.openToolbar(options);

      const system = options.requisition.system;

    // Register a one time listener with the local set of commands
      const addedDeferred = defer();
      const removedDeferred = defer();
      let state = "preAdd"; // Then 'postAdd' then 'postRemove'

      system.commands.onCommandsChange.add(function (ev) {
        if (system.commands.get("tsslow") != null) {
          if (state === "preAdd") {
            addedDeferred.resolve();
            state = "postAdd";
          }
        }
        else {
          if (state === "postAdd") {
            removedDeferred.resolve();
            state = "postRemove";
          }
        }
      });

    // Send a message to add the commands to the content process
      const front = yield GcliFront.create(options.target);
      yield front._testOnlyAddItemsByModule(MOCK_COMMANDS_URI);

    // This will cause the local set of commands to be updated with the
    // command proxies, wait for that to complete.
      yield addedDeferred.promise;

    // Now we need to add the converters to the local GCLI
      const converters = mockCommands.items.filter(item => item.item === "converter");
      system.addItems(converters);

    // Next run the tests
      yield helpers.runTests(options, exports);

    // Finally undo the mock commands and converters
      system.removeItems(converters);
      const removePromise = system.commands.onCommandsChange.once();
      yield front._testOnlyRemoveItemsByModule(MOCK_COMMANDS_URI);
      yield removedDeferred.promise;

    // And close everything down
      yield helpers.closeToolbar(options);
      yield helpers.closeTab(options);
    }).then(finish, helpers.handleError);
  };

/**
 * Ensure that the options object is setup correctly
 * options should contain an automator object that looks like this:
 * {
 *   getInputState: function() { ... },
 *   setCursor: function(cursor) { ... },
 *   getCompleterTemplateData: function() { ... },
 *   focus: function() { ... },
 *   getErrorMessage: function() { ... },
 *   fakeKey: function(keyCode) { ... },
 *   setInput: function(typed) { ... },
 *   focusManager: ...,
 *   field: ...,
 * }
 */
  function checkOptions(options) {
    if (options == null) {
      console.trace();
      throw new Error("Missing options object");
    }
    if (options.requisition == null) {
      console.trace();
      throw new Error("options.requisition == null");
    }
  }

/**
 * Various functions to return the actual state of the command line
 */
  helpers._actual = {
    input: function (options) {
      return options.automator.getInputState().typed;
    },

    hints: function (options) {
      return options.automator.getCompleterTemplateData().then(function (data) {
        var emptyParams = data.emptyParameters.join("");
        return (data.directTabText + emptyParams + data.arrowTabText)
                .replace(/\u00a0/g, " ")
                .replace(/\u21E5/, "->")
                .replace(/ $/, "");
      });
    },

    markup: function (options) {
      var cursor = helpers._actual.cursor(options);
      var statusMarkup = options.requisition.getInputStatusMarkup(cursor);
      return statusMarkup.map(function (s) {
        return new Array(s.string.length + 1).join(s.status.toString()[0]);
      }).join("");
    },

    cursor: function (options) {
      return options.automator.getInputState().cursor.start;
    },

    current: function (options) {
      var cursor = helpers._actual.cursor(options);
      return options.requisition.getAssignmentAt(cursor).param.name;
    },

    status: function (options) {
      return options.requisition.status.toString();
    },

    predictions: function (options) {
      var cursor = helpers._actual.cursor(options);
      var assignment = options.requisition.getAssignmentAt(cursor);
      var context = options.requisition.executionContext;
      return assignment.getPredictions(context).then(function (predictions) {
        return predictions.map(function (prediction) {
          return prediction.name;
        });
      });
    },

    unassigned: function (options) {
      return options.requisition._unassigned.map(assignment => {
        return assignment.arg.toString();
      });
    },

    outputState: function (options) {
      var outputData = options.automator.focusManager._shouldShowOutput();
      return outputData.visible + ":" + outputData.reason;
    },

    tooltipState: function (options) {
      var tooltipData = options.automator.focusManager._shouldShowTooltip();
      return tooltipData.visible + ":" + tooltipData.reason;
    },

    options: function (options) {
      if (options.automator.field.menu == null) {
        return [];
      }
      return options.automator.field.menu.items.map(function (item) {
        return item.name.textContent ? item.name.textContent : item.name;
      });
    },

    message: function (options) {
      return options.automator.getErrorMessage();
    }
  };

  function shouldOutputUnquoted(value) {
    var type = typeof value;
    return value == null || type === "boolean" || type === "number";
  }

  function outputArray(array) {
    return (array.length === 0) ?
      "[ ]" :
      "[ '" + array.join("', '") + "' ]";
  }

  helpers._createDebugCheck = function (options) {
    checkOptions(options);
    var requisition = options.requisition;
    var command = requisition.commandAssignment.value;
    var cursor = helpers._actual.cursor(options);
    var input = helpers._actual.input(options);
    var padding = new Array(input.length + 1).join(" ");

    var hintsPromise = helpers._actual.hints(options);
    var predictionsPromise = helpers._actual.predictions(options);

    return Promise.all([ hintsPromise, predictionsPromise ]).then(values => {
      var hints = values[0];
      var predictions = values[1];
      var output = "";

      output += "return helpers.audit(options, [\n";
      output += "  {\n";

      if (cursor === input.length) {
        output += "    setup:    '" + input + "',\n";
      }
      else {
        output += "    name: '" + input + " (cursor=" + cursor + ")',\n";
        output += "    setup: function() {\n";
        output += "      return helpers.setInput(options, '" + input + "', " + cursor + ");\n";
        output += "    },\n";
      }

      output += "    check: {\n";

      output += "      input:  '" + input + "',\n";
      output += "      hints:  " + padding + "'" + hints + "',\n";
      output += "      markup: '" + helpers._actual.markup(options) + "',\n";
      output += "      cursor: " + cursor + ",\n";
      output += "      current: '" + helpers._actual.current(options) + "',\n";
      output += "      status: '" + helpers._actual.status(options) + "',\n";
      output += "      options: " + outputArray(helpers._actual.options(options)) + ",\n";
      output += "      message: '" + helpers._actual.message(options) + "',\n";
      output += "      predictions: " + outputArray(predictions) + ",\n";
      output += "      unassigned: " + outputArray(requisition._unassigned) + ",\n";
      output += "      outputState: '" + helpers._actual.outputState(options) + "',\n";
      output += "      tooltipState: '" + helpers._actual.tooltipState(options) + "'" +
              (command ? "," : "") + "\n";

      if (command) {
        output += "      args: {\n";
        output += "        command: { name: '" + command.name + "' },\n";

        requisition.getAssignments().forEach(function (assignment) {
          output += "        " + assignment.param.name + ": { ";

          if (typeof assignment.value === "string") {
            output += "value: '" + assignment.value + "', ";
          }
          else if (shouldOutputUnquoted(assignment.value)) {
            output += "value: " + assignment.value + ", ";
          }
        else {
            output += "/*value:" + assignment.value + ",*/ ";
          }

          output += "arg: '" + assignment.arg + "', ";
          output += "status: '" + assignment.getStatus().toString() + "', ";
          output += "message: '" + assignment.message + "'";
          output += " },\n";
        });

        output += "      }\n";
      }

      output += "    },\n";
      output += "    exec: {\n";
      output += "      output: '',\n";
      output += "      type: 'string',\n";
      output += "      error: false\n";
      output += "    }\n";
      output += "  }\n";
      output += "]);";

      return output;
    }, util.errorHandler);
  };

/**
 * Simulate focusing the input field
 */
  helpers.focusInput = function (options) {
    checkOptions(options);
    options.automator.focus();
  };

/**
 * Simulate pressing TAB in the input field
 */
  helpers.pressTab = function (options) {
    checkOptions(options);
    return helpers.pressKey(options, KeyEvent.DOM_VK_TAB);
  };

/**
 * Simulate pressing RETURN in the input field
 */
  helpers.pressReturn = function (options) {
    checkOptions(options);
    return helpers.pressKey(options, KeyEvent.DOM_VK_RETURN);
  };

/**
 * Simulate pressing a key by keyCode in the input field
 */
  helpers.pressKey = function (options, keyCode) {
    checkOptions(options);
    return options.automator.fakeKey(keyCode);
  };

/**
 * A list of special key presses and how to to them, for the benefit of
 * helpers.setInput
 */
  var ACTIONS = {
    "<TAB>": function (options) {
      return helpers.pressTab(options);
    },
    "<RETURN>": function (options) {
      return helpers.pressReturn(options);
    },
    "<UP>": function (options) {
      return helpers.pressKey(options, KeyEvent.DOM_VK_UP);
    },
    "<DOWN>": function (options) {
      return helpers.pressKey(options, KeyEvent.DOM_VK_DOWN);
    },
    "<BACKSPACE>": function (options) {
      return helpers.pressKey(options, KeyEvent.DOM_VK_BACK_SPACE);
    }
  };

/**
 * Used in helpers.setInput to cut an input string like 'blah<TAB>foo<UP>' into
 * an array like [ 'blah', '<TAB>', 'foo', '<UP>' ].
 * When using this RegExp, you also need to filter out the blank strings.
 */
  var CHUNKER = /([^<]*)(<[A-Z]+>)/;

/**
 * Alter the input to <code>typed</code> optionally leaving the cursor at
 * <code>cursor</code>.
 * @return A promise of the number of key-presses to respond
 */
  helpers.setInput = function (options, typed, cursor) {
    checkOptions(options);
    var inputPromise;
    var automator = options.automator;
  // We try to measure average keypress time, but setInput can simulate
  // several, so we try to keep track of how many
    var chunkLen = 1;

  // The easy case is a simple string without things like <TAB>
    if (typed.indexOf("<") === -1) {
      inputPromise = automator.setInput(typed);
    }
    else {
    // Cut the input up into input strings separated by '<KEY>' tokens. The
    // CHUNKS RegExp leaves blanks so we filter them out.
      var chunks = typed.split(CHUNKER).filter(function (s) {
        return s !== "";
      });
      chunkLen = chunks.length + 1;

    // We're working on this in chunks so first clear the input
      inputPromise = automator.setInput("").then(function () {
        return util.promiseEach(chunks, function (chunk) {
          if (chunk.charAt(0) === "<") {
            var action = ACTIONS[chunk];
            if (typeof action !== "function") {
              console.error("Known actions: " + Object.keys(ACTIONS).join());
              throw new Error('Key action not found "' + chunk + '"');
            }
            return action(options);
          }
          else {
            return automator.setInput(automator.getInputState().typed + chunk);
          }
        });
      });
    }

    return inputPromise.then(function () {
      if (cursor != null) {
        automator.setCursor({ start: cursor, end: cursor });
      }

      if (automator.focusManager) {
        automator.focusManager.onInputChange();
      }

    // Firefox testing is noisy and distant, so logging helps
      if (options.isFirefox) {
        var cursorStr = (cursor == null ? "" : ", " + cursor);
        log('setInput("' + typed + '"' + cursorStr + ")");
      }

      return chunkLen;
    });
  };

/**
 * Helper for helpers.audit() to ensure that all the 'check' properties match.
 * See helpers.audit for more information.
 * @param name The name to use in error messages
 * @param checks See helpers.audit for a list of available checks
 * @return A promise which resolves to undefined when the checks are complete
 */
  helpers._check = function (options, name, checks) {
  // A test method to check that all args are assigned in some way
    var requisition = options.requisition;
    requisition._args.forEach(function (arg) {
      if (arg.assignment == null) {
        assert.ok(false, "No assignment for " + arg);
      }
    });

    if (checks == null) {
      return Promise.resolve();
    }

    var outstanding = [];
    var suffix = name ? " (for '" + name + "')" : "";

    if (!options.isNode && "input" in checks) {
      assert.is(helpers._actual.input(options), checks.input, "input" + suffix);
    }

    if (!options.isNode && "cursor" in checks) {
      assert.is(helpers._actual.cursor(options), checks.cursor, "cursor" + suffix);
    }

    if (!options.isNode && "current" in checks) {
      assert.is(helpers._actual.current(options), checks.current, "current" + suffix);
    }

    if ("status" in checks) {
      assert.is(helpers._actual.status(options), checks.status, "status" + suffix);
    }

    if (!options.isNode && "markup" in checks) {
      assert.is(helpers._actual.markup(options), checks.markup, "markup" + suffix);
    }

    if (!options.isNode && "hints" in checks) {
      var hintCheck = function (actualHints) {
        assert.is(actualHints, checks.hints, "hints" + suffix);
      };
      outstanding.push(helpers._actual.hints(options).then(hintCheck));
    }

    if (!options.isNode && "predictions" in checks) {
      var predictionsCheck = function (actualPredictions) {
        helpers.arrayIs(actualPredictions,
                       checks.predictions,
                       "predictions" + suffix);
      };
      outstanding.push(helpers._actual.predictions(options).then(predictionsCheck));
    }

    if (!options.isNode && "predictionsContains" in checks) {
      var containsCheck = function (actualPredictions) {
        checks.predictionsContains.forEach(function (prediction) {
          var index = actualPredictions.indexOf(prediction);
          assert.ok(index !== -1,
                  "predictionsContains:" + prediction + suffix);
          if (index === -1) {
            log("Actual predictions (" + actualPredictions.length + "): " +
              actualPredictions.join(", "));
          }
        });
      };
      outstanding.push(helpers._actual.predictions(options).then(containsCheck));
    }

    if ("unassigned" in checks) {
      helpers.arrayIs(helpers._actual.unassigned(options),
                     checks.unassigned,
                     "unassigned" + suffix);
    }

  /* TODO: Fix this
  if (!options.isNode && 'tooltipState' in checks) {
    assert.is(helpers._actual.tooltipState(options),
              checks.tooltipState,
              'tooltipState' + suffix);
  }
  */

    if (!options.isNode && "outputState" in checks) {
      assert.is(helpers._actual.outputState(options),
              checks.outputState,
              "outputState" + suffix);
    }

    if (!options.isNode && "options" in checks) {
      helpers.arrayIs(helpers._actual.options(options),
                     checks.options,
                     "options" + suffix);
    }

    if (!options.isNode && "error" in checks) {
      assert.is(helpers._actual.message(options), checks.error, "error" + suffix);
    }

    if (checks.args != null) {
      Object.keys(checks.args).forEach(function (paramName) {
        var check = checks.args[paramName];

      // We allow an 'argument' called 'command' to be the command itself, but
      // what if the command has a parameter called 'command' (for example, an
      // 'exec' command)? We default to using the parameter because checking
      // the command value is less useful
        var assignment = requisition.getAssignment(paramName);
        if (assignment == null && paramName === "command") {
          assignment = requisition.commandAssignment;
        }

        if (assignment == null) {
          assert.ok(false, "Unknown arg: " + paramName + suffix);
          return;
        }

        if ("value" in check) {
          if (typeof check.value === "function") {
            try {
              check.value(assignment.value);
            }
          catch (ex) {
            assert.ok(false, "" + ex);
          }
          }
          else {
            assert.is(assignment.value,
                    check.value,
                    "arg." + paramName + ".value" + suffix);
          }
        }

        if ("name" in check) {
          assert.is(assignment.value.name,
                  check.name,
                  "arg." + paramName + ".name" + suffix);
        }

        if ("type" in check) {
          assert.is(assignment.arg.type,
                  check.type,
                  "arg." + paramName + ".type" + suffix);
        }

        if ("arg" in check) {
          assert.is(assignment.arg.toString(),
                  check.arg,
                  "arg." + paramName + ".arg" + suffix);
        }

        if ("status" in check) {
          assert.is(assignment.getStatus().toString(),
                  check.status,
                  "arg." + paramName + ".status" + suffix);
        }

        if (!options.isNode && "message" in check) {
          if (typeof check.message.test === "function") {
            assert.ok(check.message.test(assignment.message),
                    "arg." + paramName + ".message" + suffix);
          }
          else {
            assert.is(assignment.message,
                    check.message,
                    "arg." + paramName + ".message" + suffix);
          }
        }
      });
    }

    return Promise.all(outstanding).then(function () {
    // Ensure the promise resolves to nothing
      return undefined;
    });
  };

/**
 * Helper for helpers.audit() to ensure that all the 'exec' properties work.
 * See helpers.audit for more information.
 * @param name The name to use in error messages
 * @param expected See helpers.audit for a list of available exec checks
 * @return A promise which resolves to undefined when the checks are complete
 */
  helpers._exec = function (options, name, expected) {
    var requisition = options.requisition;
    if (expected == null) {
      return Promise.resolve({});
    }

    var origLogErrors = cli.logErrors;
    if (expected.error) {
      cli.logErrors = false;
    }

    try {
      return requisition.exec({ hidden: true }).then(output => {
        if ("type" in expected) {
          assert.is(output.type,
                  expected.type,
                  "output.type for: " + name);
        }

        if ("error" in expected) {
          assert.is(output.error,
                  expected.error,
                  "output.error for: " + name);
        }

        if (!("output" in expected)) {
          return { output: output };
        }

        var context = requisition.conversionContext;
        var convertPromise;
        if (options.isNode) {
          convertPromise = output.convert("string", context);
        }
        else {
          convertPromise = output.convert("dom", context).then(function (node) {
            return (node == null) ? "" : node.textContent.trim();
          });
        }

        return convertPromise.then(function (textOutput) {
          // Test that a regular expression has at least one match in a string.
          var doTest = function (match, against) {
          // Only log the real textContent if the test fails
            if (against.match(match) != null) {
              assert.ok(true,
                `html output for '${name}' should match /${match.source || match}/`);
            } else {
              assert.ok(false,
                `html output for '${name}' should match /${match.source || match}/. ` +
                `Actual textContent: "${against}"`);
            }
          };

          // Test that a regular expression has no matches in a string.
          var doTestNot = function (match, against) {
          // Only log the real textContent if the test fails
            if (against.match(match) != null) {
              assert.ok(false,
                `html output for '${name}' should not match /` +
                `${match.source || match}/. Actual textContent: "${against}"`);
            } else {
              assert.ok(true,
              `html output for '${name}' should not match /${match.source || match}/`);
            }
          };

          if (typeof expected.output === "string") {
            assert.is(textOutput,
                    expected.output,
                    `html output for '${name}'`);
          } else if (Array.isArray(expected.output)) {
            expected.output.forEach(function (match) {
              doTest(match, textOutput);
            });
          } else {
            doTest(expected.output, textOutput);
          }

          if (typeof expected.notinoutput === "string") {
            assert.ok(textOutput.indexOf(expected.notinoutput) === -1,
              `html output for "${name}" doesn't contain "${expected.notinoutput}"`);
          } else if (Array.isArray(expected.notinoutput)) {
            expected.notinoutput.forEach(function (match) {
              doTestNot(match, textOutput);
            });
          } else if (typeof expected.notinoutput !== "undefined") {
            doTestNot(expected.notinoutput, textOutput);
          }

          if (expected.error) {
            cli.logErrors = origLogErrors;
          }
          return { output: output, text: textOutput };
        });
      }).then(function (data) {
        if (expected.error) {
          cli.logErrors = origLogErrors;
        }

        return data;
      });
    }
  catch (ex) {
    assert.ok(false, "Failure executing '" + name + "': " + ex);
    util.errorHandler(ex);

    if (expected.error) {
      cli.logErrors = origLogErrors;
    }
    return Promise.resolve({});
  }
  };

/**
 * Helper to setup the test
 */
  helpers._setup = function (options, name, audit) {
    if (typeof audit.setup === "string") {
      return helpers.setInput(options, audit.setup);
    }

    if (typeof audit.setup === "function") {
      return Promise.resolve(audit.setup.call(audit));
    }

    return Promise.reject("'setup' property must be a string or a function. Is " + audit.setup);
  };

/**
 * Helper to shutdown the test
 */
  helpers._post = function (name, audit, data) {
    if (typeof audit.post === "function") {
      return Promise.resolve(audit.post.call(audit, data.output, data.text));
    }
    return Promise.resolve(audit.post);
  };

/*
 * We do some basic response time stats so we can see if we're getting slow
 */
  var totalResponseTime = 0;
  var averageOver = 0;
  var maxResponseTime = 0;
  var maxResponseCulprit;
  var start;

/**
 * Restart the stats collection process
 */
  helpers.resetResponseTimes = function () {
    start = new Date().getTime();
    totalResponseTime = 0;
    averageOver = 0;
    maxResponseTime = 0;
    maxResponseCulprit = undefined;
  };

/**
 * Expose an average response time in milliseconds
 */
  Object.defineProperty(helpers, "averageResponseTime", {
    get: function () {
      return averageOver === 0 ?
        undefined :
        Math.round(100 * totalResponseTime / averageOver) / 100;
    },
    enumerable: true
  });

/**
 * Expose a maximum response time in milliseconds
 */
  Object.defineProperty(helpers, "maxResponseTime", {
    get: function () { return Math.round(maxResponseTime * 100) / 100; },
    enumerable: true
  });

/**
 * Expose the name of the test that provided the maximum response time
 */
  Object.defineProperty(helpers, "maxResponseCulprit", {
    get: function () { return maxResponseCulprit; },
    enumerable: true
  });

/**
 * Quick summary of the times
 */
  Object.defineProperty(helpers, "timingSummary", {
    get: function () {
      var elapsed = (new Date().getTime() - start) / 1000;
      return "Total " + elapsed + "s, " +
           "ave response " + helpers.averageResponseTime + "ms, " +
           "max response " + helpers.maxResponseTime + "ms " +
           "from '" + helpers.maxResponseCulprit + "'";
    },
    enumerable: true
  });

/**
 * A way of turning a set of tests into something more declarative, this helps
 * to allow tests to be asynchronous.
 * @param audits An array of objects each of which contains:
 * - setup: string/function to be called to set the test up.
 *     If audit is a string then it is passed to helpers.setInput().
 *     If audit is a function then it is executed. The tests will wait while
 *     tests that return promises complete.
 * - name: For debugging purposes. If name is undefined, and 'setup'
 *     is a string then the setup value will be used automatically
 * - skipIf: A function to define if the test should be skipped. Useful for
 *     excluding tests from certain environments (e.g. nodom, firefox, etc).
 *     The name of the test will be used in log messages noting the skip
 *     See helpers.reason for pre-defined skip functions. The skip function must
 *     be synchronous, and will be passed the test options object.
 * - skipRemainingIf: A function to skip all the remaining audits in this set.
 *     See skipIf for details of how skip functions work.
 * - check: Check data. Available checks:
 *   - input: The text displayed in the input field
 *   - cursor: The position of the start of the cursor
 *   - status: One of 'VALID', 'ERROR', 'INCOMPLETE'
 *   - hints: The hint text, i.e. a concatenation of the directTabText, the
 *       emptyParameters and the arrowTabText. The text as inserted into the UI
 *       will include NBSP and Unicode RARR characters, these should be
 *       represented using normal space and '->' for the arrow
 *   - markup: What state should the error markup be in. e.g. 'VVVIIIEEE'
 *   - args: Maps of checks to make against the arguments:
 *     - value: i.e. assignment.value (which ignores defaultValue)
 *     - type: Argument/BlankArgument/MergedArgument/etc i.e. what's assigned
 *             Care should be taken with this since it's something of an
 *             implementation detail
 *     - arg: The toString value of the argument
 *     - status: i.e. assignment.getStatus
 *     - message: i.e. assignment.message
 *     - name: For commands - checks assignment.value.name
 * - exec: Object to indicate we should execute the command and check the
 *     results. Available checks:
 *   - output: A string, RegExp or array of RegExps to compare with the output
 *       If typeof output is a string then the output should be exactly equal
 *       to the given string. If the type of output is a RegExp or array of
 *       RegExps then the output should match all RegExps
 *   - error: If true, then it is expected that this command will fail (that
 *       is, return a rejected promise or throw an exception)
 *   - type: A string documenting the expected type of the return value
 * - post: Function to be called after the checks have been run, which will be
 *     passed 2 parameters: the first being output data (with type, data, and
 *     error properties), and the second being the converted text version of
 *     the output data
 */
  helpers.audit = function (options, audits) {
    checkOptions(options);
    var skipReason = null;
    return util.promiseEach(audits, function (audit) {
      var name = audit.name;
      if (name == null && typeof audit.setup === "string") {
        name = audit.setup;
      }

      if (assert.testLogging) {
        log("- START '" + name + "' in " + assert.currentTest);
      }

      if (audit.skipRemainingIf) {
        var skipRemainingIf = (typeof audit.skipRemainingIf === "function") ?
          audit.skipRemainingIf(options) :
          !!audit.skipRemainingIf;
        if (skipRemainingIf) {
          skipReason = audit.skipRemainingIf.name ?
            "due to " + audit.skipRemainingIf.name :
            "";
          assert.log("Skipped " + name + " " + skipReason);

        // Tests need at least one pass, fail or todo. Create a dummy pass
          assert.ok(true, "Each test requires at least one pass, fail or todo");

          return Promise.resolve(undefined);
        }
      }

      if (audit.skipIf) {
        var skip = (typeof audit.skipIf === "function") ?
          audit.skipIf(options) :
          !!audit.skipIf;
        if (skip) {
          var reason = audit.skipIf.name ? "due to " + audit.skipIf.name : "";
          assert.log("Skipped " + name + " " + reason);
          return Promise.resolve(undefined);
        }
      }

      if (skipReason != null) {
        assert.log("Skipped " + name + " " + skipReason);
        return Promise.resolve(undefined);
      }

      var start = new Date().getTime();

      var setupDone = helpers._setup(options, name, audit);
      return setupDone.then(function (chunkLen) {
        if (typeof chunkLen !== "number") {
          chunkLen = 1;
        }

      // Nasty hack to allow us to auto-skip tests where we're actually testing
      // a key-sequence (i.e. targeting terminal.js) when there is no terminal
        if (chunkLen === -1) {
          assert.log("Skipped " + name + " " + skipReason);
          return Promise.resolve(undefined);
        }

        if (assert.currentTest) {
          var responseTime = (new Date().getTime() - start) / chunkLen;
          totalResponseTime += responseTime;
          if (responseTime > maxResponseTime) {
            maxResponseTime = responseTime;
            maxResponseCulprit = assert.currentTest + "/" + name;
          }
          averageOver++;
        }

        var checkDone = helpers._check(options, name, audit.check);
        return checkDone.then(function () {
          var execDone = helpers._exec(options, name, audit.exec);
          return execDone.then(function (data) {
            return helpers._post(name, audit, data).then(function () {
              if (assert.testLogging) {
                log("- END '" + name + "' in " + assert.currentTest);
              }
            });
          });
        });
      });
    }).then(function () {
      return options.automator.setInput("");
    }, function (ex) {
      options.automator.setInput("");
      throw ex;
    });
  };

/**
 * Compare 2 arrays.
 */
  helpers.arrayIs = function (actual, expected, message) {
    assert.ok(Array.isArray(actual), "actual is not an array: " + message);
    assert.ok(Array.isArray(expected), "expected is not an array: " + message);

    if (!Array.isArray(actual) || !Array.isArray(expected)) {
      return;
    }

    assert.is(actual.length, expected.length, "array length: " + message);

    for (var i = 0; i < actual.length && i < expected.length; i++) {
      assert.is(actual[i], expected[i], "member[" + i + "]: " + message);
    }
  };

/**
 * A quick helper to log to the correct place
 */
  function log(message) {
    if (typeof info === "function") {
      info(message);
    }
    else {
      console.log(message);
    }
  }

  return { helpers: helpers, assert: assert };
})();
