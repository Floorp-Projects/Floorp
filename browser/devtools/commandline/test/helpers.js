/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 *
 *  DO NOT ALTER THIS FILE WITHOUT KEEPING IT IN SYNC WITH THE OTHER COPIES
 *  OF THIS FILE.
 *
 *  UNAUTHORIZED ALTERATION WILL RESULT IN THE ALTEREE BEING SENT TO SIT ON
 *  THE NAUGHTY STEP.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *  FOR A LONG TIME.
 *
 */


/*
 * Use as a JSM
 * ------------
 * helpers._createDebugCheck() and maybe other functions in this file can be
 * useful at runtime, so it is possible to use helpers.js as a JSM.
 * Copy commandline/test/helpers.js to shared/helpers.jsm, and then add to
 * DeveloperToolbar.jsm the following:
 *
 * XPCOMUtils.defineLazyModuleGetter(this, "helpers",
 *                                 "resource:///modules/devtools/helpers.jsm");
 *
 * At the bottom of DeveloperToolbar.prototype._onload add this:
 *
 * var options = { display: this.display };
 * this._input.onkeypress = function(ev) {
 *   helpers.setup(options);
 *   dump(helpers._createDebugCheck() + '\n\n');
 * };
 *
 * Now GCLI will emit output on every keypress that both explains the state
 * of GCLI and can be run as a test case.
 */

this.EXPORTED_SYMBOLS = [ 'helpers' ];

var test = { };

/**
 * Various functions for testing DeveloperToolbar.
 * Parts of this code exist in:
 * - browser/devtools/commandline/test/head.js
 * - browser/devtools/shared/test/head.js
 */
let DeveloperToolbarTest = { };

/**
 * Paranoid DeveloperToolbar.show();
 */
DeveloperToolbarTest.show = function DTT_show(aCallback) {
  if (DeveloperToolbar.visible) {
    ok(false, "DeveloperToolbar.visible at start of openDeveloperToolbar");
  }
  else {
    DeveloperToolbar.show(true, aCallback);
  }
};

/**
 * Paranoid DeveloperToolbar.hide();
 */
DeveloperToolbarTest.hide = function DTT_hide() {
  if (!DeveloperToolbar.visible) {
    ok(false, "!DeveloperToolbar.visible at start of closeDeveloperToolbar");
  }
  else {
    DeveloperToolbar.display.inputter.setInput("");
    DeveloperToolbar.hide();
  }
};

/**
 * check() is the new status. Similar API except that it doesn't attempt to
 * alter the display/requisition at all, and it makes extra checks.
 * Test inputs
 *   typed: The text to type at the input
 * Available checks:
 *   input: The text displayed in the input field
 *   cursor: The position of the start of the cursor
 *   status: One of "VALID", "ERROR", "INCOMPLETE"
 *   emptyParameters: Array of parameters still to type. e.g. [ "<message>" ]
 *   directTabText: Simple completion text
 *   arrowTabText: When the completion is not an extension (without arrow)
 *   markup: What state should the error markup be in. e.g. "VVVIIIEEE"
 *   args: Maps of checks to make against the arguments:
 *     value: i.e. assignment.value (which ignores defaultValue)
 *     type: Argument/BlankArgument/MergedArgument/etc i.e. what's assigned
 *           Care should be taken with this since it's something of an
 *           implementation detail
 *     arg: The toString value of the argument
 *     status: i.e. assignment.getStatus
 *     message: i.e. assignment.getMessage
 *     name: For commands - checks assignment.value.name
 */
DeveloperToolbarTest.checkInputStatus = function DTT_checkInputStatus(checks) {
  if (!checks.emptyParameters) {
    checks.emptyParameters = [];
  }
  if (!checks.directTabText) {
    checks.directTabText = '';
  }
  if (!checks.arrowTabText) {
    checks.arrowTabText = '';
  }

  var display = DeveloperToolbar.display;

  if (checks.typed) {
    info('Starting tests for ' + checks.typed);
    display.inputter.setInput(checks.typed);
  }
  else {
    ok(false, "Missing typed for " + JSON.stringify(checks));
    return;
  }

  if (checks.cursor) {
    display.inputter.setCursor(checks.cursor)
  }

  var cursor = checks.cursor ? checks.cursor.start : checks.typed.length;

  var requisition = display.requisition;
  var completer = display.completer;
  var actual = completer._getCompleterTemplateData();

  /*
  if (checks.input) {
    is(display.inputter.element.value,
            checks.input,
            'input');
  }

  if (checks.cursor) {
    is(display.inputter.element.selectionStart,
            checks.cursor,
            'cursor');
  }
  */

  if (checks.status) {
    is(requisition.getStatus().toString(),
            checks.status,
            'status');
  }

  if (checks.markup) {
    var statusMarkup = requisition.getInputStatusMarkup(cursor);
    var actualMarkup = statusMarkup.map(function(s) {
      return Array(s.string.length + 1).join(s.status.toString()[0]);
    }).join('');

    is(checks.markup,
            actualMarkup,
            'markup');
  }

  if (checks.emptyParameters) {
    var actualParams = actual.emptyParameters;
    is(actualParams.length,
            checks.emptyParameters.length,
            'emptyParameters.length');

    if (actualParams.length === checks.emptyParameters.length) {
      for (var i = 0; i < actualParams.length; i++) {
        is(actualParams[i].replace(/\u00a0/g, ' '),
                checks.emptyParameters[i],
                'emptyParameters[' + i + ']');
      }
    }
    else {
      info('Expected: [ \"' + actualParams.join('", "') + '" ]');
    }
  }

  if (checks.directTabText) {
    is(actual.directTabText,
            checks.directTabText,
            'directTabText');
  }

  if (checks.arrowTabText) {
    is(actual.arrowTabText,
            ' \u00a0\u21E5 ' + checks.arrowTabText,
            'arrowTabText');
  }

  if (checks.args) {
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
        ok(false, 'Unknown parameter: ' + paramName);
        return;
      }

      if (check.value) {
        is(assignment.value,
                check.value,
                'checkStatus value for ' + paramName);
      }

      if (check.name) {
        is(assignment.value.name,
                check.name,
                'checkStatus name for ' + paramName);
      }

      if (check.type) {
        is(assignment.arg.type,
                check.type,
                'checkStatus type for ' + paramName);
      }

      if (check.arg) {
        is(assignment.arg.toString(),
                check.arg,
                'checkStatus arg for ' + paramName);
      }

      if (check.status) {
        is(assignment.getStatus().toString(),
                check.status,
                'checkStatus status for ' + paramName);
      }

      if (check.message) {
        is(assignment.getMessage(),
                check.message,
                'checkStatus message for ' + paramName);
      }
    });
  }
};

/**
 * Execute a command:
 *
 * DeveloperToolbarTest.exec({
 *   // Test inputs
 *   typed: "echo hi",        // Optional, uses existing if undefined
 *
 *   // Thing to check
 *   args: { message: "hi" }, // Check that the args were understood properly
 *   outputMatch: /^hi$/,     // RegExp to test against textContent of output
 *                            // (can also be array of RegExps)
 *   blankOutput: true,       // Special checks when there is no output
 * });
 */
DeveloperToolbarTest.exec = function DTT_exec(tests) {
  tests = tests || {};

  if (tests.typed) {
    DeveloperToolbar.display.inputter.setInput(tests.typed);
  }

  let typed = DeveloperToolbar.display.inputter.getInputState().typed;
  let output = DeveloperToolbar.display.requisition.exec();

  is(typed, output.typed, 'output.command for: ' + typed);

  if (tests.completed !== false) {
    ok(output.completed, 'output.completed false for: ' + typed);
  }
  else {
    // It is actually an error if we say something is async and it turns
    // out not to be? For now we're saying 'no'
    // ok(!output.completed, 'output.completed true for: ' + typed);
  }

  if (tests.args != null) {
    is(Object.keys(tests.args).length, Object.keys(output.args).length,
       'arg count for ' + typed);

    Object.keys(output.args).forEach(function(arg) {
      let expectedArg = tests.args[arg];
      let actualArg = output.args[arg];

      if (typeof expectedArg === 'function') {
        ok(expectedArg(actualArg), 'failed test func. ' + typed + '/' + arg);
      }
      else {
        if (Array.isArray(expectedArg)) {
          if (!Array.isArray(actualArg)) {
            ok(false, 'actual is not an array. ' + typed + '/' + arg);
            return;
          }

          is(expectedArg.length, actualArg.length,
                  'array length: ' + typed + '/' + arg);
          for (let i = 0; i < expectedArg.length; i++) {
            is(expectedArg[i], actualArg[i],
                    'member: "' + typed + '/' + arg + '/' + i);
          }
        }
        else {
          is(expectedArg, actualArg, 'typed: "' + typed + '" arg: ' + arg);
        }
      }
    });
  }

  let displayed = DeveloperToolbar.outputPanel._div.textContent;

  if (tests.outputMatch) {
    var doTest = function(match, against) {
      if (!match.test(against)) {
        info("Actual textContent: '" + against + "'");
        ok(false, "mismatched output for " + typed + " against " + match.source +
                " (textContent sent to info)");
      }
    }
    if (Array.isArray(tests.outputMatch)) {
      tests.outputMatch.forEach(function(match) {
        doTest(match, displayed);
      });
    }
    else {
      doTest(tests.outputMatch, displayed);
    }
  }

  if (tests.blankOutput != null) {
    if (!/^$/.test(displayed)) {
      info("Actual textContent: '" + displayed + "'");
      ok(false, "non-blank output for " + typed + " (textContent sent to info)");
    }
  }

  return output;
};

/**
 * Quick wrapper around the things you need to do to run DeveloperToolbar
 * command tests:
 * - Set the pref 'devtools.toolbar.enabled' to true
 * - Add a tab pointing at |uri|
 * - Open the DeveloperToolbar
 * - Register a cleanup function to undo the above
 * - Run the tests
 *
 * @param uri The uri of a page to load. Can be 'about:blank' or 'data:...'
 * @param target Either a function or array of functions containing the tests
 * to run. If an array of test function is passed then we will clear up after
 * the tests have completed. If a single test function is passed then this
 * function should arrange for 'finish()' to be called on completion.
 */
DeveloperToolbarTest.test = function DTT_test(uri, target, isGcli) {
  let menuItem = document.getElementById("menu_devToolbar");
  let command = document.getElementById("Tools:DevToolbar");
  let appMenuItem = document.getElementById("appmenu_devToolbar");

  registerCleanupFunction(function() {
    DeveloperToolbarTest.hide();

    // a.k.a Services.prefs.clearUserPref("devtools.toolbar.enabled");
    if (menuItem) {
      menuItem.hidden = true;
    }
    if (command) {
      command.setAttribute("disabled", "true");
    }
    if (appMenuItem) {
      appMenuItem.hidden = true;
    }

    // leakHunt({ DeveloperToolbar: DeveloperToolbar });
  });

  // a.k.a: Services.prefs.setBoolPref("devtools.toolbar.enabled", true);
  if (menuItem) {
    menuItem.hidden = false;
  }
  if (command) {
    command.removeAttribute("disabled");
  }
  if (appMenuItem) {
    appMenuItem.hidden = false;
  }

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  content.location = uri;

  let tab = gBrowser.selectedTab;
  let browser = gBrowser.getBrowserForTab(tab);

  var onTabLoad = function() {
    browser.removeEventListener("load", onTabLoad, true);

    DeveloperToolbarTest.show(function() {
      var options = {
        window: content,
        display: DeveloperToolbar.display,
        isFirefox: true
      };

      if (helpers && !isGcli) {
        helpers.setup(options);
      }

      if (Array.isArray(target)) {
        try {
          target.forEach(function(func) {
            var args = isGcli ? [ options ] : [ browser, tab ];
            func.apply(null, args);
          })
        }
        finally {
          DeveloperToolbarTest._checkFinish();
        }
      }
      else {
        try {
          target(browser, tab);
        }
        catch (ex) {
console.error(ex.stack);
          ok(false, "" + ex);
          DeveloperToolbarTest._finish();
          throw ex;
        }
      }
    });
  }

  browser.addEventListener("load", onTabLoad, true);
};

DeveloperToolbarTest._outstanding = [];

DeveloperToolbarTest._checkFinish = function() {
  info('_checkFinish. ' + DeveloperToolbarTest._outstanding.length + ' outstanding');
  if (DeveloperToolbarTest._outstanding.length == 0) {
    DeveloperToolbarTest._finish();
  }
}

DeveloperToolbarTest._finish = function() {
  info('Finish');
  DeveloperToolbarTest.closeAllTabs();
  finish();
}

DeveloperToolbarTest.checkCalled = function(aFunc, aScope) {
  var todo = function() {
    var reply = aFunc.apply(aScope, arguments);
    DeveloperToolbarTest._outstanding = DeveloperToolbarTest._outstanding.filter(function(aJob) {
      return aJob != todo;
    });
    DeveloperToolbarTest._checkFinish();
    return reply;
  }
  DeveloperToolbarTest._outstanding.push(todo);
  return todo;
};

DeveloperToolbarTest.checkNotCalled = function(aMsg, aFunc, aScope) {
  return function() {
    ok(false, aMsg);
    return aFunc.apply(aScope, arguments);
  }
};

/**
 *
 */
DeveloperToolbarTest.closeAllTabs = function() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
};

///////////////////////////////////////////////////////////////////////////////

this.helpers = {};

var assert = { ok: ok, is: is, log: info };

helpers._display = undefined;
helpers._options = undefined;

helpers.setup = function(options) {
  helpers._options = options;
  helpers._display = options.display;
};

helpers.shutdown = function(options) {
  helpers._options = undefined;
  helpers._display = undefined;
};

/**
 * Various functions to return the actual state of the command line
 */
helpers._actual = {
  input: function() {
    return helpers._display.inputter.element.value;
  },

  hints: function() {
    var templateData = helpers._display.completer._getCompleterTemplateData();
    var actualHints = templateData.directTabText +
                      templateData.emptyParameters.join('') +
                      templateData.arrowTabText;
    return actualHints.replace(/\u00a0/g, ' ')
                      .replace(/\u21E5/, '->')
                      .replace(/ $/, '');
  },

  markup: function() {
    var cursor = helpers._display.inputter.element.selectionStart;
    var statusMarkup = helpers._display.requisition.getInputStatusMarkup(cursor);
    return statusMarkup.map(function(s) {
      return Array(s.string.length + 1).join(s.status.toString()[0]);
    }).join('');
  },

  cursor: function() {
    return helpers._display.inputter.element.selectionStart;
  },

  current: function() {
    return helpers._display.requisition.getAssignmentAt(helpers._actual.cursor()).param.name;
  },

  status: function() {
    return helpers._display.requisition.getStatus().toString();
  },

  outputState: function() {
    var outputData = helpers._display.focusManager._shouldShowOutput();
    return outputData.visible + ':' + outputData.reason;
  },

  tooltipState: function() {
    var tooltipData = helpers._display.focusManager._shouldShowTooltip();
    return tooltipData.visible + ':' + tooltipData.reason;
  }
};

helpers._directToString = [ 'boolean', 'undefined', 'number' ];

helpers._createDebugCheck = function() {
  var requisition = helpers._display.requisition;
  var command = requisition.commandAssignment.value;
  var input = helpers._actual.input();
  var padding = Array(input.length + 1).join(' ');

  var output = '';
  output += 'helpers.setInput(\'' + input + '\');\n';
  output += 'helpers.check({\n';
  output += '  input:  \'' + input + '\',\n';
  output += '  hints:  ' + padding + '\'' + helpers._actual.hints() + '\',\n';
  output += '  markup: \'' + helpers._actual.markup() + '\',\n';
  output += '  cursor: ' + helpers._actual.cursor() + ',\n';
  output += '  current: \'' + helpers._actual.current() + '\',\n';
  output += '  status: \'' + helpers._actual.status() + '\',\n';
  output += '  outputState: \'' + helpers._actual.outputState() + '\',\n';

  if (command) {
    output += '  tooltipState: \'' + helpers._actual.tooltipState() + '\',\n';
    output += '  args: {\n';
    output += '    command: { name: \'' + command.name + '\' },\n';

    requisition.getAssignments().forEach(function(assignment) {
      output += '    ' + assignment.param.name + ': { ';

      if (typeof assignment.value === 'string') {
        output += 'value: \'' + assignment.value + '\', ';
      }
      else if (helpers._directToString.indexOf(typeof assignment.value) !== -1) {
        output += 'value: ' + assignment.value + ', ';
      }
      else if (assignment.value === null) {
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

    output += '  }\n';
  }
  else {
    output += '  tooltipState: \'' + helpers._actual.tooltipState() + '\'\n';
  }
  output += '});';

  return output;
};

/**
 * We're splitting status into setup() which alters the state of the system
 * and check() which ensures that things are in the right place afterwards.
 */
helpers.setInput = function(typed, cursor) {
  helpers._display.inputter.setInput(typed);

  if (cursor) {
    helpers._display.inputter.setCursor({ start: cursor, end: cursor });
  }
  else {
    // This is a hack because jsdom appears to not handle cursor updates
    // in the same way as most browsers.
    if (helpers._options.isJsdom) {
      helpers._display.inputter.setCursor({
        start: typed.length,
        end: typed.length
      });
    }
  }

  helpers._display.focusManager.onInputChange();

  // Firefox testing is noisy and distant, so logging helps
  if (helpers._options.isFirefox) {
    assert.log('setInput("' + typed + '"' + (cursor == null ? '' : ', ' + cursor) + ')');
  }
};

/**
 * Simulate focusing the input field
 */
helpers.focusInput = function() {
  helpers._display.inputter.focus();
};

/**
 * Simulate pressing TAB in the input field
 */
helpers.pressTab = function() {
  helpers.pressKey(9 /*KeyEvent.DOM_VK_TAB*/);
};

/**
 * Simulate pressing RETURN in the input field
 */
helpers.pressReturn = function() {
  helpers.pressKey(13 /*KeyEvent.DOM_VK_RETURN*/);
};

/**
 * Simulate pressing a key by keyCode in the input field
 */
helpers.pressKey = function(keyCode) {
  var fakeEvent = {
    keyCode: keyCode,
    preventDefault: function() { },
    timeStamp: new Date().getTime()
  };
  helpers._display.inputter.onKeyDown(fakeEvent);
  helpers._display.inputter.onKeyUp(fakeEvent);
};

/**
 * check() is the new status. Similar API except that it doesn't attempt to
 * alter the display/requisition at all, and it makes extra checks.
 * Available checks:
 *   input: The text displayed in the input field
 *   cursor: The position of the start of the cursor
 *   status: One of "VALID", "ERROR", "INCOMPLETE"
 *   hints: The hint text, i.e. a concatenation of the directTabText, the
 *     emptyParameters and the arrowTabText. The text as inserted into the UI
 *     will include NBSP and Unicode RARR characters, these should be
 *     represented using normal space and '->' for the arrow
 *   markup: What state should the error markup be in. e.g. "VVVIIIEEE"
 *   args: Maps of checks to make against the arguments:
 *     value: i.e. assignment.value (which ignores defaultValue)
 *     type: Argument/BlankArgument/MergedArgument/etc i.e. what's assigned
 *           Care should be taken with this since it's something of an
 *           implementation detail
 *     arg: The toString value of the argument
 *     status: i.e. assignment.getStatus
 *     message: i.e. assignment.getMessage
 *     name: For commands - checks assignment.value.name
 */
helpers.check = function(checks) {
  if ('input' in checks) {
    assert.is(helpers._actual.input(), checks.input, 'input');
  }

  if ('cursor' in checks) {
    assert.is(helpers._actual.cursor(), checks.cursor, 'cursor');
  }

  if ('current' in checks) {
    assert.is(helpers._actual.current(), checks.current, 'current');
  }

  if ('status' in checks) {
    assert.is(helpers._actual.status(), checks.status, 'status');
  }

  if ('markup' in checks) {
    assert.is(helpers._actual.markup(), checks.markup, 'markup');
  }

  if ('hints' in checks) {
    assert.is(helpers._actual.hints(), checks.hints, 'hints');
  }

  if ('tooltipState' in checks) {
    assert.is(helpers._actual.tooltipState(), checks.tooltipState, 'tooltipState');
  }

  if ('outputState' in checks) {
    assert.is(helpers._actual.outputState(), checks.outputState, 'outputState');
  }

  if (checks.args != null) {
    var requisition = helpers._display.requisition;
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
        assert.ok(false, 'Unknown arg: ' + paramName);
        return;
      }

      if ('value' in check) {
        assert.is(assignment.value,
                check.value,
                'arg.' + paramName + '.value');
      }

      if ('name' in check) {
        assert.is(assignment.value.name,
                check.name,
                'arg.' + paramName + '.name');
      }

      if ('type' in check) {
        assert.is(assignment.arg.type,
                check.type,
                'arg.' + paramName + '.type');
      }

      if ('arg' in check) {
        assert.is(assignment.arg.toString(),
                check.arg,
                'arg.' + paramName + '.arg');
      }

      if ('status' in check) {
        assert.is(assignment.getStatus().toString(),
                check.status,
                'arg.' + paramName + '.status');
      }

      if ('message' in check) {
        assert.is(assignment.getMessage(),
                check.message,
                'arg.' + paramName + '.message');
      }
    });
  }
};

/**
 * Execute a command:
 *
 * helpers.exec({
 *   // Test inputs
 *   typed: "echo hi",        // Optional, uses existing if undefined
 *
 *   // Thing to check
 *   args: { message: "hi" }, // Check that the args were understood properly
 *   outputMatch: /^hi$/,     // Regex to test against textContent of output
 *   blankOutput: true,       // Special checks when there is no output
 * });
 */
helpers.exec = function(tests) {
  var requisition = helpers._display.requisition;
  var inputter = helpers._display.inputter;

  tests = tests || {};

  if (tests.typed) {
    inputter.setInput(tests.typed);
  }

  var typed = inputter.getInputState().typed;
  var output = requisition.exec({ hidden: true });

  assert.is(typed, output.typed, 'output.command for: ' + typed);

  if (tests.completed !== false) {
    assert.ok(output.completed, 'output.completed false for: ' + typed);
  }
  else {
    // It is actually an error if we say something is async and it turns
    // out not to be? For now we're saying 'no'
    // assert.ok(!output.completed, 'output.completed true for: ' + typed);
  }

  if (tests.args != null) {
    assert.is(Object.keys(tests.args).length, Object.keys(output.args).length,
            'arg count for ' + typed);

    Object.keys(output.args).forEach(function(arg) {
      var expectedArg = tests.args[arg];
      var actualArg = output.args[arg];

      if (Array.isArray(expectedArg)) {
        if (!Array.isArray(actualArg)) {
          assert.ok(false, 'actual is not an array. ' + typed + '/' + arg);
          return;
        }

        assert.is(expectedArg.length, actualArg.length,
                'array length: ' + typed + '/' + arg);
        for (var i = 0; i < expectedArg.length; i++) {
          assert.is(expectedArg[i], actualArg[i],
                  'member: "' + typed + '/' + arg + '/' + i);
        }
      }
      else {
        assert.is(expectedArg, actualArg, 'typed: "' + typed + '" arg: ' + arg);
      }
    });
  }

  if (!helpers._options.window.document.createElement) {
    assert.log('skipping output tests (missing doc.createElement) for ' + typed);
    return;
  }

  var div = helpers._options.window.document.createElement('div');
  output.toDom(div);
  var displayed = div.textContent.trim();

  if (tests.outputMatch) {
    var doTest = function(match, against) {
      if (!match.test(against)) {
        assert.ok(false, "html output for " + typed + " against " + match.source);
        console.log("Actual textContent");
        console.log(against);
      }
    }
    if (Array.isArray(tests.outputMatch)) {
      tests.outputMatch.forEach(function(match) {
        doTest(match, displayed);
      });
    }
    else {
      doTest(tests.outputMatch, displayed);
    }
  }

  if (tests.blankOutput != null) {
    if (!/^$/.test(displayed)) {
      assert.ok(false, "html for " + typed + " (textContent sent to info)");
      console.log("Actual textContent");
      console.log(displayed);
    }
  }
};
