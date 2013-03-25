/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Use as a JSM
// ------------
// helpers._createDebugCheck() can be useful at runtime. To use as a JSM, copy
// commandline/test/helpers.js to shared/helpers.jsm, and then import to
// DeveloperToolbar.jsm with:
//   XPCOMUtils.defineLazyModuleGetter(this, "helpers",
//                                 "resource:///modules/devtools/helpers.jsm");
// At the bottom of DeveloperToolbar.prototype._onload add this:
//   var options = { display: this.display };
//   this._input.onkeypress = function(ev) {
//     helpers.setup(options);
//     dump(helpers._createDebugCheck() + '\n\n');
//   };
// Now GCLI will emit output on every keypress that both explains the state
// of GCLI and can be run as a test case.

this.EXPORTED_SYMBOLS = [ 'helpers' ];
var helpers = {};
this.helpers = helpers;
let require = (Cu.import("resource://gre/modules/devtools/Require.jsm", {})).require;
Components.utils.import("resource:///modules/devtools/gcli.jsm", {});

let console = (Cu.import("resource://gre/modules/devtools/Console.jsm", {})).console;
let TargetFactory = (Cu.import("resource:///modules/devtools/Target.jsm", {})).TargetFactory;

let Promise = (Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {})).Promise;
let assert = { ok: ok, is: is, log: info };

var util = require('util/util');

var converters = require('gcli/converters');

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
 * - tab: The new XUL tab element, as returned by gBrowser.addTab()
 * - target: The debug target as defined by the devtools framework
 * - browser: The XUL browser element for the given tab
 * - window: Content window for the created tab. a.k.a 'content' in mochitest
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
helpers.addTab = function(url, callback, options) {
  var deferred = Promise.defer();

  waitForExplicitFinish();

  options = options || {};
  options.chromeWindow = options.chromeWindow || window;
  options.isFirefox = true;

  var tabbrowser = options.chromeWindow.gBrowser;
  options.tab = tabbrowser.addTab();
  tabbrowser.selectedTab = options.tab;
  options.browser = tabbrowser.getBrowserForTab(options.tab);
  options.target = TargetFactory.forTab(options.tab);

  var onPageLoad = function() {
    options.browser.removeEventListener("load", onPageLoad, true);
    options.document = options.browser.contentDocument;
    options.window = options.document.defaultView;

    var cleanUp = function() {
      tabbrowser.removeTab(options.tab);

      delete options.window;
      delete options.document;

      delete options.target;
      delete options.browser;
      delete options.tab;

      delete options.chromeWindow;
      delete options.isFirefox;

      deferred.resolve();
    };

    var reply = callback(options);
    Promise.resolve(reply).then(cleanUp, function(error) {
      ok(false, error);
      cleanUp();
    });
  };

  options.browser.contentWindow.location = url;
  options.browser.addEventListener("load", onPageLoad, true);

  return deferred.promise;
};

/**
 * Warning: For use with Firefox Mochitests only.
 *
 * As addTab, but that also opens the developer toolbar. In addition a new
 * 'display' property is added to the options object with the display from GCLI
 * in the developer toolbar
 */
helpers.addTabWithToolbar = function(url, callback, options) {
  return helpers.addTab(url, function(innerOptions) {
    var win = innerOptions.chromeWindow;
    var deferred = Promise.defer();

    win.DeveloperToolbar.show(true, function() {
      innerOptions.display = win.DeveloperToolbar.display;

      var cleanUp = function() {
        win.DeveloperToolbar.hide();
        delete innerOptions.display;
        deferred.resolve();
      };

      var reply = callback(innerOptions);
      Promise.resolve(reply).then(cleanUp, function(error) {
        ok(false, error);
        cleanUp();
      });
    });
    return deferred.promise;
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
helpers.runTests = function(options, tests) {
  var testNames = Object.keys(tests).filter(function(test) {
    return test != "setup" && test != "shutdown";
  });

  var recover = function(error) {
    console.error(error);
  };

  info("SETUP");
  var setupDone = (tests.setup != null) ?
      Promise.resolve(tests.setup(options)) :
      Promise.resolve();

  var testDone = setupDone.then(function() {
    return util.promiseEach(testNames, function(testName) {
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

  return testDone.then(function() {
    info("SHUTDOWN");
    return (tests.shutdown != null) ?
        Promise.resolve(tests.shutdown(options)) :
        Promise.resolve();
  }, recover);
};

///////////////////////////////////////////////////////////////////////////////

function checkOptions(options) {
  if (options == null) {
    console.trace();
    throw new Error('Missing options object');
  }
  if (options.display == null) {
    console.trace();
    throw new Error('options object does not contain a display property');
  }
  if (options.display.requisition == null) {
    console.trace();
    throw new Error('display object does not contain a requisition');
  }
}

/**
 * Various functions to return the actual state of the command line
 */
helpers._actual = {
  input: function(options) {
    return options.display.inputter.element.value;
  },

  hints: function(options) {
    var templateData = options.display.completer._getCompleterTemplateData();
    var join = function(directTabText, emptyParameters, arrowTabText) {
      return (directTabText + emptyParameters.join('') + arrowTabText)
                .replace(/\u00a0/g, ' ')
                .replace(/\u21E5/, '->')
                .replace(/ $/, '');
    };

    var promisedJoin = util.promised(join);
    return promisedJoin(templateData.directTabText,
                        templateData.emptyParameters,
                        templateData.arrowTabText);
  },

  markup: function(options) {
    var cursor = options.display.inputter.element.selectionStart;
    var statusMarkup = options.display.requisition.getInputStatusMarkup(cursor);
    return statusMarkup.map(function(s) {
      return Array(s.string.length + 1).join(s.status.toString()[0]);
    }).join('');
  },

  cursor: function(options) {
    return options.display.inputter.element.selectionStart;
  },

  current: function(options) {
    return options.display.requisition.getAssignmentAt(helpers._actual.cursor(options)).param.name;
  },

  status: function(options) {
    return options.display.requisition.getStatus().toString();
  },

  predictions: function(options) {
    var cursor = options.display.inputter.element.selectionStart;
    var assignment = options.display.requisition.getAssignmentAt(cursor);
    return assignment.getPredictions().then(function(predictions) {
      return predictions.map(function(prediction) {
        return prediction.name;
      });
    });
  },

  unassigned: function(options) {
    return options.display.requisition._unassigned.map(function(assignment) {
      return assignment.arg.toString();
    }.bind(this));
  },

  outputState: function(options) {
    var outputData = options.display.focusManager._shouldShowOutput();
    return outputData.visible + ':' + outputData.reason;
  },

  tooltipState: function(options) {
    var tooltipData = options.display.focusManager._shouldShowTooltip();
    return tooltipData.visible + ':' + tooltipData.reason;
  },

  options: function(options) {
    if (options.display.tooltip.field.menu == null) {
      return [];
    }
    return options.display.tooltip.field.menu.items.map(function(item) {
      return item.name.textContent ? item.name.textContent : item.name;
    });
  },

  message: function(options) {
    return options.display.tooltip.errorEle.textContent;
  }
};

function shouldOutputUnquoted(value) {
  var type = typeof value;
  return value == null || type === 'boolean' || type === 'number';
}

function outputArray(array) {
  return (array.length === 0) ?
      '[ ]' :
      '[ \'' + array.join('\', \'') + '\' ]';
}

helpers._createDebugCheck = function(options) {
  checkOptions(options);
  var requisition = options.display.requisition;
  var command = requisition.commandAssignment.value;
  var cursor = helpers._actual.cursor(options);
  var input = helpers._actual.input(options);
  var padding = Array(input.length + 1).join(' ');

  var hintsPromise = helpers._actual.hints(options);
  var predictionsPromise = helpers._actual.predictions(options);

  return util.all(hintsPromise, predictionsPromise).then(function(values) {
    var hints = values[0];
    var predictions = values[1];
    var output = '';

    output += 'helpers.audit(options, [\n';
    output += '  {\n';

    if (cursor === input.length) {
      output += '    setup:    \'' + input + '\',\n';
    }
    else {
      output += '    name: \'' + input + ' (cursor=' + cursor + ')\',\n';
      output += '    setup: function() {\n';
      output += '      return helpers.setInput(options, \'' + input + '\', ' + cursor + ');\n';
      output += '    },\n';
    }

    output += '    check: {\n';

    output += '      input:  \'' + input + '\',\n';
    output += '      hints:  ' + padding + '\'' + hints + '\',\n';
    output += '      markup: \'' + helpers._actual.markup(options) + '\',\n';
    output += '      cursor: ' + cursor + ',\n';
    output += '      current: \'' + helpers._actual.current(options) + '\',\n';
    output += '      status: \'' + helpers._actual.status(options) + '\',\n';
    output += '      options: ' + outputArray(helpers._actual.options(options)) + ',\n';
    output += '      error: \'' + helpers._actual.message(options) + '\',\n';
    output += '      predictions: ' + outputArray(predictions) + ',\n';
    output += '      unassigned: ' + outputArray(requisition._unassigned) + ',\n';
    output += '      outputState: \'' + helpers._actual.outputState(options) + '\',\n';
    output += '      tooltipState: \'' + helpers._actual.tooltipState(options) + '\'' +
              (command ? ',' : '') +'\n';

    if (command) {
      output += '      args: {\n';
      output += '        command: { name: \'' + command.name + '\' },\n';

      requisition.getAssignments().forEach(function(assignment) {
        output += '        ' + assignment.param.name + ': { ';

        if (typeof assignment.value === 'string') {
          output += 'value: \'' + assignment.value + '\', ';
        }
        else if (shouldOutputUnquoted(assignment.value)) {
          output += 'value: ' + assignment.value + ', ';
        }
        else {
          output += '/*value:' + assignment.value + ',*/ ';
        }

        output += 'arg: \'' + assignment.arg + '\', ';
        output += 'status: \'' + assignment.getStatus().toString() + '\', ';
        output += 'message: \'' + assignment.getMessage() + '\'';
        output += ' },\n';
      });

      output += '      }\n';
    }

    output += '    },\n';
    output += '    exec: {\n';
    output += '      output: \'\',\n';
    output += '      completed: true,\n';
    output += '    }\n';
    output += '  }\n';
    output += ']);';

    return output;
  }.bind(this), util.errorHandler);
};

/**
 * Simulate focusing the input field
 */
helpers.focusInput = function(options) {
  checkOptions(options);
  options.display.inputter.focus();
};

/**
 * Simulate pressing TAB in the input field
 */
helpers.pressTab = function(options) {
  checkOptions(options);
  return helpers.pressKey(options, 9 /*KeyEvent.DOM_VK_TAB*/);
};

/**
 * Simulate pressing RETURN in the input field
 */
helpers.pressReturn = function(options) {
  checkOptions(options);
  return helpers.pressKey(options, 13 /*KeyEvent.DOM_VK_RETURN*/);
};

/**
 * Simulate pressing a key by keyCode in the input field
 */
helpers.pressKey = function(options, keyCode) {
  checkOptions(options);
  var fakeEvent = {
    keyCode: keyCode,
    preventDefault: function() { },
    timeStamp: new Date().getTime()
  };
  options.display.inputter.onKeyDown(fakeEvent);
  return options.display.inputter.handleKeyUp(fakeEvent);
};

/**
 * A list of special key presses and how to to them, for the benefit of
 * helpers.setInput
 */
var ACTIONS = {
  '<TAB>': function(options) {
    return helpers.pressTab(options);
  },
  '<RETURN>': function(options) {
    return helpers.pressReturn(options);
  },
  '<UP>': function(options) {
    return helpers.pressKey(options, 38 /*KeyEvent.DOM_VK_UP*/);
  },
  '<DOWN>': function(options) {
    return helpers.pressKey(options, 40 /*KeyEvent.DOM_VK_DOWN*/);
  }
};

/**
 * Used in helpers.setInput to cut an input string like "blah<TAB>foo<UP>" into
 * an array like [ "blah", "<TAB>", "foo", "<UP>" ].
 * When using this RegExp, you also need to filter out the blank strings.
 */
var CHUNKER = /([^<]*)(<[A-Z]+>)/;

/**
 * Alter the input to <code>typed</code> optionally leaving the cursor at
 * <code>cursor</code>.
 * @return A promise of the number of key-presses to respond
 */
helpers.setInput = function(options, typed, cursor) {
  checkOptions(options);
  var promise = undefined;
  var inputter = options.display.inputter;
  // We try to measure average keypress time, but setInput can simulate
  // several, so we try to keep track of how many
  var chunkLen = 1;

  // The easy case is a simple string without things like <TAB>
  if (typed.indexOf('<') === -1) {
    promise = inputter.setInput(typed);
  }
  else {
    // Cut the input up into input strings separated by '<KEY>' tokens. The
    // CHUNKS RegExp leaves blanks so we filter them out.
    var chunks = typed.split(CHUNKER).filter(function(s) {
      return s != '';
    });
    chunkLen = chunks.length + 1;

    // We're working on this in chunks so first clear the input
    promise = inputter.setInput('').then(function() {
      return util.promiseEach(chunks, function(chunk) {
        if (chunk.charAt(0) === '<') {
          var action = ACTIONS[chunk];
          if (typeof action !== 'function') {
            console.error('Known actions: ' + Object.keys(ACTIONS).join());
            throw new Error('Key action not found "' + chunk + '"');
          }
          return action(options);
        }
        else {
          return inputter.setInput(inputter.element.value + chunk);
        }
      });
    });
  }

  return promise.then(function() {
    if (cursor != null) {
      options.display.inputter.setCursor({ start: cursor, end: cursor });
    }
    else {
      // This is a hack because jsdom appears to not handle cursor updates
      // in the same way as most browsers.
      if (options.isJsdom) {
        options.display.inputter.setCursor({
          start: typed.length,
          end: typed.length
        });
      }
    }

    options.display.focusManager.onInputChange();

    // Firefox testing is noisy and distant, so logging helps
    if (options.isFirefox) {
      var cursorStr = (cursor == null ? '' : ', ' + cursor);
      log('setInput("' + typed + '"' + cursorStr + ')');
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
helpers._check = function(options, name, checks) {
  if (checks == null) {
    return Promise.resolve();
  }

  var outstanding = [];
  var suffix = name ? ' (for \'' + name + '\')' : '';

  if ('input' in checks) {
    assert.is(helpers._actual.input(options), checks.input, 'input' + suffix);
  }

  if ('cursor' in checks) {
    assert.is(helpers._actual.cursor(options), checks.cursor, 'cursor' + suffix);
  }

  if ('current' in checks) {
    assert.is(helpers._actual.current(options), checks.current, 'current' + suffix);
  }

  if ('status' in checks) {
    assert.is(helpers._actual.status(options), checks.status, 'status' + suffix);
  }

  if ('markup' in checks) {
    assert.is(helpers._actual.markup(options), checks.markup, 'markup' + suffix);
  }

  if ('hints' in checks) {
    var hintCheck = function(actualHints) {
      assert.is(actualHints, checks.hints, 'hints' + suffix);
    };
    outstanding.push(helpers._actual.hints(options).then(hintCheck));
  }

  if ('predictions' in checks) {
    var predictionsCheck = function(actualPredictions) {
      helpers._arrayIs(actualPredictions,
                       checks.predictions,
                       'predictions' + suffix);
    };
    outstanding.push(helpers._actual.predictions(options).then(predictionsCheck));
  }

  if ('predictionsContains' in checks) {
    var containsCheck = function(actualPredictions) {
      checks.predictionsContains.forEach(function(prediction) {
        var index = actualPredictions.indexOf(prediction);
        assert.ok(index !== -1,
                  'predictionsContains:' + prediction + suffix);
      });
    };
    outstanding.push(helpers._actual.predictions(options).then(containsCheck));
  }

  if ('unassigned' in checks) {
    helpers._arrayIs(helpers._actual.unassigned(options),
                     checks.unassigned,
                     'unassigned' + suffix);
  }

  if ('tooltipState' in checks) {
    if (options.isJsdom) {
      assert.log('Skipped ' + name + '/tooltipState due to jsdom');
    }
    else {
      assert.is(helpers._actual.tooltipState(options),
                checks.tooltipState,
                'tooltipState' + suffix);
    }
  }

  if ('outputState' in checks) {
    if (options.isJsdom) {
      assert.log('Skipped ' + name + '/outputState due to jsdom');
    }
    else {
      assert.is(helpers._actual.outputState(options),
                checks.outputState,
                'outputState' + suffix);
    }
  }

  if ('options' in checks) {
    helpers._arrayIs(helpers._actual.options(options),
                     checks.options,
                     'options' + suffix);
  }

  if ('error' in checks) {
    assert.is(helpers._actual.message(options), checks.error, 'error' + suffix);
  }

  if (checks.args != null) {
    var requisition = options.display.requisition;
    Object.keys(checks.args).forEach(function(paramName) {
      var check = checks.args[paramName];

      var assignment;
      if (paramName === 'command') {
        assignment = requisition.commandAssignment;
      }
      else {
        assignment = requisition.getAssignment(paramName);
      }

      if (assignment == null) {
        assert.ok(false, 'Unknown arg: ' + paramName + suffix);
        return;
      }

      if ('value' in check) {
        assert.is(assignment.value,
                  check.value,
                  'arg.' + paramName + '.value' + suffix);
      }

      if ('name' in check) {
        if (options.isJsdom) {
          assert.log('Skipped arg.' + paramName + '.name due to jsdom');
        }
        else {
          assert.is(assignment.value.name,
                    check.name,
                    'arg.' + paramName + '.name' + suffix);
        }
      }

      if ('type' in check) {
        assert.is(assignment.arg.type,
                  check.type,
                  'arg.' + paramName + '.type' + suffix);
      }

      if ('arg' in check) {
        assert.is(assignment.arg.toString(),
                  check.arg,
                  'arg.' + paramName + '.arg' + suffix);
      }

      if ('status' in check) {
        assert.is(assignment.getStatus().toString(),
                  check.status,
                  'arg.' + paramName + '.status' + suffix);
      }

      if ('message' in check) {
        if (typeof check.message.test === 'function') {
          assert.ok(check.message.test(assignment.getMessage()),
                    'arg.' + paramName + '.message' + suffix);
        }
        else {
          assert.is(assignment.getMessage(),
                    check.message,
                    'arg.' + paramName + '.message' + suffix);
        }
      }
    });
  }

  return util.all(outstanding).then(function() {
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
helpers._exec = function(options, name, expected) {
  if (expected == null) {
    return Promise.resolve();
  }

  var output = options.display.requisition.exec({ hidden: true });

  if ('completed' in expected) {
    assert.is(output.completed,
              expected.completed,
              'output.completed false for: ' + name);
  }

  if (!options.window.document.createElement) {
    assert.log('skipping output tests (missing doc.createElement) for ' + name);
    return Promise.resolve();
  }

  if (!('output' in expected)) {
    return Promise.resolve();
  }

  var deferred = Promise.defer();

  var checkOutput = function() {
    var div = options.window.document.createElement('div');
    var nodePromise = converters.convert(output.data, output.type, 'dom',
                                         options.display.requisition.context);
    nodePromise.then(function(node) {
      div.appendChild(node);
      var actualOutput = div.textContent.trim();

      var doTest = function(match, against) {
        if (!match.test(against)) {
          assert.ok(false, 'html output for ' + name + ' against ' + match.source);
          log('Actual textContent');
          log(against);
        }
      };

      if (typeof expected.output === 'string') {
        assert.is(actualOutput,
                  expected.output,
                  'html output for ' + name);
      }
      else if (Array.isArray(expected.output)) {
        expected.output.forEach(function(match) {
          doTest(match, actualOutput);
        });
      }
      else {
        doTest(expected.output, actualOutput);
      }

      deferred.resolve();
    });
  };

  if (output.completed !== false) {
    checkOutput();
  }
  else {
    var changed = function() {
      if (output.completed !== false) {
        checkOutput();
        output.onChange.remove(changed);
      }
    };
    output.onChange.add(changed);
  }

  return deferred.promise;
};

/**
 * Helper to setup the test
 */
helpers._setup = function(options, name, action) {
  if (typeof action === 'string') {
    return helpers.setInput(options, action);
  }

  if (typeof action === 'function') {
    return Promise.resolve(action());
  }

  return Promise.reject('setup must be a string or a function');
};

/**
 * Helper to shutdown the test
 */
helpers._post = function(name, action) {
  if (typeof action === 'function') {
    return Promise.resolve(action());
  }
  return Promise.resolve(action);
};

/*
 * We do some basic response time stats so we can see if we're getting slow
 */
var totalResponseTime = 0;
var averageOver = 0;
var maxResponseTime = 0;
var maxResponseCulprit = undefined;

/**
 * Restart the stats collection process
 */
helpers.resetResponseTimes = function() {
  totalResponseTime = 0;
  averageOver = 0;
  maxResponseTime = 0;
  maxResponseCulprit = undefined;
};

/**
 * Expose an average response time in milliseconds
 */
Object.defineProperty(helpers, 'averageResponseTime', {
  get: function() {
    return averageOver === 0 ?
        undefined :
        Math.round(100 * totalResponseTime / averageOver) / 100;
  },
  enumerable: true
});

/**
 * Expose a maximum response time in milliseconds
 */
Object.defineProperty(helpers, 'maxResponseTime', {
  get: function() { return Math.round(maxResponseTime * 100) / 100; },
  enumerable: true
});

/**
 * Expose the name of the test that provided the maximum response time
 */
Object.defineProperty(helpers, 'maxResponseCulprit', {
  get: function() { return maxResponseCulprit; },
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
 *     excluding tests from certain environments (e.g. jsdom, firefox, etc).
 *     The name of the test will be used in log messages noting the skip
 *     See helpers.reason for pre-defined skip functions. The skip function must
 *     be synchronous, and will be passed the test options object.
 * - skipRemainingIf: A function to skip all the remaining audits in this set.
 *     See skipIf for details of how skip functions work.
 * - check: Check data. Available checks:
 *   - input: The text displayed in the input field
 *   - cursor: The position of the start of the cursor
 *   - status: One of "VALID", "ERROR", "INCOMPLETE"
 *   - hints: The hint text, i.e. a concatenation of the directTabText, the
 *       emptyParameters and the arrowTabText. The text as inserted into the UI
 *       will include NBSP and Unicode RARR characters, these should be
 *       represented using normal space and '->' for the arrow
 *   - markup: What state should the error markup be in. e.g. "VVVIIIEEE"
 *   - args: Maps of checks to make against the arguments:
 *     - value: i.e. assignment.value (which ignores defaultValue)
 *     - type: Argument/BlankArgument/MergedArgument/etc i.e. what's assigned
 *             Care should be taken with this since it's something of an
 *             implementation detail
 *     - arg: The toString value of the argument
 *     - status: i.e. assignment.getStatus
 *     - message: i.e. assignment.getMessage
 *     - name: For commands - checks assignment.value.name
 * - exec: Object to indicate we should execute the command and check the
 *     results. Available checks:
 *   - output: A string, RegExp or array of RegExps to compare with the output
 *       If typeof output is a string then the output should be exactly equal
 *       to the given string. If the type of output is a RegExp or array of
 *       RegExps then the output should match all RegExps
 *   - completed: A boolean which declares that we should check to see if the
 *       command completed synchronously
 * - post: Function to be called after the checks have been run
 */
helpers.audit = function(options, audits) {
  checkOptions(options);
  var skipReason = null;
  return util.promiseEach(audits, function(audit) {
    var name = audit.name;
    if (name == null && typeof audit.setup === 'string') {
      name = audit.setup;
    }

    if (assert.testLogging) {
      log('- START \'' + name + '\' in ' + assert.currentTest);
    }

    if (audit.skipIf) {
      var skip = (typeof audit.skipIf === 'function') ?
          audit.skipIf(options) :
          !!audit.skipIf;
      if (skip) {
        var reason = audit.skipIf.name ? 'due to ' + audit.skipIf.name : '';
        assert.log('Skipped ' + name + ' ' + reason);
        return Promise.resolve(undefined);
      }
    }

    if (audit.skipRemainingIf) {
      var skipRemainingIf = (typeof audit.skipRemainingIf === 'function') ?
          audit.skipRemainingIf(options) :
          !!audit.skipRemainingIf;
      if (skipRemainingIf) {
        skipReason = audit.skipRemainingIf.name ?
            'due to ' + audit.skipRemainingIf.name :
            '';
        assert.log('Skipped ' + name + ' ' + skipReason);
        return Promise.resolve(undefined);
      }
    }

    if (skipReason != null) {
      assert.log('Skipped ' + name + ' ' + skipReason);
      return Promise.resolve(undefined);
    }

    var start = new Date().getTime();

    var setupDone = helpers._setup(options, name, audit.setup);
    return setupDone.then(function(chunkLen) {

      if (typeof chunkLen !== 'number') {
        chunkLen = 1;
      }
      var responseTime = (new Date().getTime() - start) / chunkLen;
      totalResponseTime += responseTime;
      if (responseTime > maxResponseTime) {
        maxResponseTime = responseTime;
        maxResponseCulprit = assert.currentTest + '/' + name;
      }
      averageOver++;

      var checkDone = helpers._check(options, name, audit.check);
      return checkDone.then(function() {
        var execDone = helpers._exec(options, name, audit.exec);
        return execDone.then(function() {
          return helpers._post(name, audit.post).then(function() {
            if (assert.testLogging) {
              log('- END \'' + name + '\' in ' + assert.currentTest);
            }
          });
        });
      });
    });
  }).then(null, function(ex) {
    console.error(ex.stack);
    throw(ex);
  });
};

/**
 * Compare 2 arrays.
 */
helpers._arrayIs = function(actual, expected, message) {
  assert.ok(Array.isArray(actual), 'actual is not an array: ' + message);
  assert.ok(Array.isArray(expected), 'expected is not an array: ' + message);

  if (!Array.isArray(actual) || !Array.isArray(expected)) {
    return;
  }

  assert.is(actual.length, expected.length, 'array length: ' + message);

  for (var i = 0; i < actual.length && i < expected.length; i++) {
    assert.is(actual[i], expected[i], 'member[' + i + ']: ' + message);
  }
};

/**
 * A quick helper to log to the correct place
 */
function log(message) {
  if (typeof info === 'function') {
    info(message);
  }
  else {
    console.log(message);
  }
}

//});
