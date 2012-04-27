/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Walker <jwalker@mozilla.com> (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let gcli;
let console;
let require;
let define;

(function() {
  let tempScope = {};
  Components.utils.import("resource:///modules/gcli.jsm", tempScope);

  gcli = tempScope.gcli;
  console = gcli._internal.console;
  define = gcli._internal.define;
  require = gcli._internal.require;
})();

/**
 * Open a new tab at a URL and call a callback on load
 */
function addTab(aURL, aCallback)
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  content.location = aURL;

  let tab = gBrowser.selectedTab;
  let browser = gBrowser.getBrowserForTab(tab);

  function onTabLoad() {
    browser.removeEventListener("load", onTabLoad, true);
    aCallback(browser, tab);
  }

  browser.addEventListener("load", onTabLoad, true);
}

registerCleanupFunction(function tearDown() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }

  console = undefined;
  define = undefined;
  require = undefined;
  gcli = undefined;
});

/**
 * Various functions for testing DeveloperToolbar
 */
let DeveloperToolbarTest = {
  /**
   * Paranoid DeveloperToolbar.show();
   */
  show: function DTT_show() {
    if (DeveloperToolbar.visible) {
      ok(false, "DeveloperToolbar.visible at start of openDeveloperToolbar");
    }
    else {
      DeveloperToolbar.show();
    }
  },

  /**
   * Paranoid DeveloperToolbar.hide();
   */
  hide: function DTT_hide() {
    if (!DeveloperToolbar.visible) {
      ok(false, "!DeveloperToolbar.visible at start of closeDeveloperToolbar");
    }
    else {
      DeveloperToolbar.display.inputter.setInput("");
      DeveloperToolbar.hide();
    }
  },

  /**
   * Check that we can parse command input.
   * Doesn't execute the command, just checks that we grok the input properly:
   * 
   * DeveloperToolbarTest.checkInputStatus({
   *   // Test inputs
   *   typed: "ech",           // Required
   *   cursor: 3,              // Optional cursor position
   *
   *   // Thing to check
   *   status: "INCOMPLETE",   // One of "VALID", "ERROR", "INCOMPLETE"
   *   emptyParameters: [ "<message>" ], // Still to type
   *   directTabText: "o",     // Simple completion text
   *   arrowTabText: "",       // When the completion is not an extension
   * });
   */
  checkInputStatus: function DTT_checkInputStatus(test) {
    if (test.typed) {
      DeveloperToolbar.display.inputter.setInput(test.typed);
    }
    else {
     ok(false, "Missing typed for " + JSON.stringify(test));
     return;
    }

    if (test.cursor) {
      DeveloperToolbar.display.inputter.setCursor(test.cursor)
    }

    if (test.status) {
      is(DeveloperToolbar.display.requisition.getStatus().toString(),
         test.status,
         "status for " + test.typed);
    }

    if (test.emptyParameters == null) {
      test.emptyParameters = [];
    }

    let completer = DeveloperToolbar.display.completer;
    let realParams = completer.emptyParameters;
    is(realParams.length, test.emptyParameters.length,
       'emptyParameters.length for \'' + test.typed + '\'');

    if (realParams.length === test.emptyParameters.length) {
      for (let i = 0; i < realParams.length; i++) {
        is(realParams[i].replace(/\u00a0/g, ' '), test.emptyParameters[i],
           'emptyParameters[' + i + '] for \'' + test.typed + '\'');
      }
    }

    if (test.directTabText) {
      is(completer.directTabText, test.directTabText,
         'directTabText for \'' + test.typed + '\'');
    }
    else {
      is(completer.directTabText, '', 'directTabText for \'' + test.typed + '\'');
    }

    if (test.arrowTabText) {
      is(completer.arrowTabText, ' \u00a0\u21E5 ' + test.arrowTabText,
         'arrowTabText for \'' + test.typed + '\'');
    }
    else {
      is(completer.arrowTabText, '', 'arrowTabText for \'' + test.typed + '\'');
    }
  },

  /**
   * Execute a command:
   *
   * DeveloperToolbarTest.exec({
   *   // Test inputs
   *   typed: "echo hi",        // Optional, uses existing if undefined
   *
   *   // Thing to check
   *   args: { message: "hi" }, // Check that the args were understood properly
   *   outputMatch: /^hi$/,     // Regex to test against textContent of output
   *   blankOutput: true,       // Special checks when there is no output
   * });
   */
  exec: function DTT_exec(test) {
    test = test || {};

    if (test.typed) {
      DeveloperToolbar.display.inputter.setInput(test.typed);
    }

    let typed = DeveloperToolbar.display.inputter.getInputState().typed;
    let output = DeveloperToolbar.display.requisition.exec();

    is(typed, output.typed, 'output.command for: ' + typed);

    if (test.completed !== false) {
      ok(output.completed, 'output.completed false for: ' + typed);
    }
    else {
      // It is actually an error if we say something is async and it turns
      // out not to be? For now we're saying 'no'
      // ok(!output.completed, 'output.completed true for: ' + typed);
    }

    if (test.args != null) {
      is(Object.keys(test.args).length, Object.keys(output.args).length,
         'arg count for ' + typed);

      Object.keys(output.args).forEach(function(arg) {
        let expectedArg = test.args[arg];
        let actualArg = output.args[arg];

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
      });
    }

    let displayed = DeveloperToolbar.outputPanel._div.textContent;

    if (test.outputMatch) {
      if (!test.outputMatch.test(displayed)) {
        ok(false, "html output for " + typed + " (textContent sent to info)");
        info("Actual textContent");
        info(displayed);
      }
    }

    if (test.blankOutput != null) {
      if (!/^$/.test(displayed)) {
        ok(false, "html output for " + typed + " (textContent sent to info)");
        info("Actual textContent");
        info(displayed);
      }
    }
  },

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
   * @param testFunc A function containing the tests to run. This should
   * arrange for 'finish()' to be called on completion.
   */
  test: function DTT_test(uri, testFunc) {
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

    addTab(uri, function(browser, tab) {
      DeveloperToolbarTest.show();
      testFunc(browser, tab);
    });
  },
};
