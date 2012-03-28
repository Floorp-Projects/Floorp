/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view displays the properties of objects.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_frame-parameters.html";

var gPane = null;
var gTab = null;
var gDebugger = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.debuggerWindow;

    testFrameParameters();
  });
}

function testFrameParameters()
{
  // scriptsadded is fired last when switching to a paused state, so the
  // property view will have had a chance to fetch the call parameters.
  gPane.activeThread.addOneTimeListener("scriptsadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.Stackframes._frames,
          localScope = gDebugger.DebuggerView.Properties.localScope,
          localNodes = localScope.querySelector(".details").childNodes;

      is(gDebugger.StackFrames.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 3,
        "Should have three frames.");

      is(localNodes.length, 8,
        "The localScope should contain all the created variable elements.");

      is(localNodes[0].querySelector(".info").textContent, "[object Proxy]",
        "Should have the right property value for 'this'.");

      // Expand the __proto__ and arguments tree nodes. This causes their
      // properties to be retrieved and displayed.
      localNodes[0].expand();
      localNodes[1].expand();

      // Poll every few milliseconds until the properties are retrieved.
      // It's important to set the timer in the chrome window, because the
      // content window timers are disabled while the debuggee is paused.
      let count = 0;
      let intervalID = window.setInterval(function(){
        if (++count > 50) {
          ok(false, "Timed out while polling for the properties.");
          resumeAndFinish();
        }
        if (!localNodes[0].fetched || !localNodes[1].fetched) {
          return;
        }
        window.clearInterval(intervalID);
        is(localNodes[0].querySelector(".property > .title > .key")
                        .textContent, "__proto__ ",
          "Should have the right property name for __proto__.");

        ok(localNodes[0].querySelector(".property > .title > .value")
                        .textContent.search(/object/) != -1,
          "__proto__ should be an object.");

        is(localNodes[1].querySelector(".info").textContent, "[object Arguments]",
          "Should have the right property value for 'arguments'.");

        is(localNodes[1].querySelector(".property > .title > .key")
                        .textContent, "length",
          "Should have the right property name for length.");

        is(localNodes[1].querySelector(".property > .title > .value")
                        .textContent, 5,
          "Should have the right argument length.");

        resumeAndFinish();
      }, 100);
    }}, 0);
  });

  EventUtils.synthesizeMouseAtCenter(content.document.querySelector("button"),
                                     {},
                                     content.window);
}

function resumeAndFinish() {
  gPane.activeThread.addOneTimeListener("framescleared", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      var frames = gDebugger.DebuggerView.Stackframes._frames;

      is(frames.querySelectorAll(".dbg-stackframe").length, 0,
        "Should have no frames.");

      closeDebuggerAndFinish(gTab);
    }}, 0);
  });

  gDebugger.StackFrames.activeThread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
