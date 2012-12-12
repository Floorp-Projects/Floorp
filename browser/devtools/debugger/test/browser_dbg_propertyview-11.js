/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view correctly displays WebIDL attributes in DOM
 * objects.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_frame-parameters.html";

let gPane = null;
let gTab = null;
let gDebugger = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    gDebugger.DebuggerController.StackFrames.autoScopeExpand = true;
    gDebugger.DebuggerView.Variables.nonEnumVisible = true;
    testFrameParameters();
  });
}

function testFrameParameters()
{
  let count = 0;
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    // We expect 2 Debugger:FetchedVariables events, one from the global object
    // scope and the regular one.
    if (++count < 2) {
      info("Number of received Debugger:FetchedVariables events: " + count);
      return;
    }
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      let anonymousScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".variables-view-scope")[1],
          globalScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".variables-view-scope")[2],
          anonymousNodes = anonymousScope.querySelector(".variables-view-element-details").childNodes,
          globalNodes = globalScope.querySelector(".variables-view-element-details").childNodes,
          gVars = gDebugger.DebuggerView.Variables;


      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(anonymousNodes[1].querySelector(".name").getAttribute("value"), "button",
        "Should have the right property name for |button|.");

      is(anonymousNodes[1].querySelector(".value").getAttribute("value"), "[object HTMLButtonElement]",
        "Should have the right property value for |button|.");

      is(anonymousNodes[2].querySelector(".name").getAttribute("value"), "buttonAsProto",
        "Should have the right property name for |buttonAsProto|.");

      is(anonymousNodes[2].querySelector(".value").getAttribute("value"), "[object Object]",
        "Should have the right property value for |buttonAsProto|.");

      is(globalNodes[3].querySelector(".name").getAttribute("value"), "document",
        "Should have the right property name for |document|.");

      is(globalNodes[3].querySelector(".value").getAttribute("value"), "[object Proxy]",
        "Should have the right property value for |document|.");

      let buttonNode = gVars.getItemForNode(anonymousNodes[1]);
      let buttonAsProtoNode = gVars.getItemForNode(anonymousNodes[2]);
      let documentNode = gVars.getItemForNode(globalNodes[3]);

      is(buttonNode.expanded, false,
        "The buttonNode should not be expanded at this point.");
      is(buttonAsProtoNode.expanded, false,
        "The buttonAsProtoNode should not be expanded at this point.");
      is(documentNode.expanded, false,
        "The documentNode should not be expanded at this point.");

      // Expand the 'button', 'buttonAsProto' and 'document' tree nodes. This
      // causes their properties to be retrieved and displayed.
      buttonNode.expand();
      buttonAsProtoNode.expand();
      documentNode.expand();

      is(buttonNode.expanded, true,
        "The buttonNode should be expanded at this point.");
      is(buttonAsProtoNode.expanded, true,
        "The buttonAsProtoNode should be expanded at this point.");
      is(documentNode.expanded, true,
        "The documentNode should be expanded at this point.");

      // Poll every few milliseconds until the properties are retrieved.
      // It's important to set the timer in the chrome window, because the
      // content window timers are disabled while the debuggee is paused.
      let count1 = 0;
      let intervalID = window.setInterval(function(){
        info("count1: " + count1);
        if (++count1 > 50) {
          ok(false, "Timed out while polling for the properties.");
          window.clearInterval(intervalID);
          return resumeAndFinish();
        }
        if (!buttonNode._retrieved ||
            !buttonAsProtoNode._retrieved ||
            !documentNode._retrieved) {
          return;
        }
        window.clearInterval(intervalID);

        // Test the prototypes of these objects.
        is(buttonNode.get("__proto__").target.querySelector(".name")
           .getAttribute("value"), "__proto__",
          "Should have the right property name for '__proto__' in buttonNode.");
        ok(buttonNode.get("__proto__").target.querySelector(".value")
           .getAttribute("value").search(/object/) != -1,
          "'__proto__' in buttonNode should be an object.");

        is(buttonAsProtoNode.get("__proto__").target.querySelector(".name")
           .getAttribute("value"), "__proto__",
          "Should have the right property name for '__proto__' in buttonAsProtoNode.");
        ok(buttonAsProtoNode.get("__proto__").target.querySelector(".value")
           .getAttribute("value").search(/object/) != -1,
          "'__proto__' in buttonAsProtoNode should be an object.");

        is(documentNode.get("__proto__").target.querySelector(".name")
           .getAttribute("value"), "__proto__",
          "Should have the right property name for '__proto__' in documentNode.");
        ok(documentNode.get("__proto__").target.querySelector(".value")
           .getAttribute("value").search(/object/) != -1,
          "'__proto__' in documentNode should be an object.");

        let buttonProtoNode = buttonNode.get("__proto__");
        let buttonAsProtoProtoNode = buttonAsProtoNode.get("__proto__");
        let documentProtoNode = documentNode.get("__proto__");

        is(buttonProtoNode.expanded, false,
          "The buttonProtoNode should not be expanded at this point.");
        is(buttonAsProtoProtoNode.expanded, false,
          "The buttonAsProtoProtoNode should not be expanded at this point.");
        is(documentProtoNode.expanded, false,
          "The documentProtoNode should not be expanded at this point.");

        // Expand the prototypes of 'button', 'buttonAsProto' and 'document'
        // tree nodes. This causes their properties to be retrieved and
        // displayed.
        buttonProtoNode.expand();
        buttonAsProtoProtoNode.expand();
        documentProtoNode.expand();

        is(buttonProtoNode.expanded, true,
          "The buttonProtoNode should be expanded at this point.");
        is(buttonAsProtoProtoNode.expanded, true,
          "The buttonAsProtoProtoNode should be expanded at this point.");
        is(documentProtoNode.expanded, true,
          "The documentProtoNode should be expanded at this point.");


        // Poll every few milliseconds until the properties are retrieved.
        // It's important to set the timer in the chrome window, because the
        // content window timers are disabled while the debuggee is paused.
        let count2 = 0;
        let intervalID1 = window.setInterval(function(){
          info("count2: " + count2);
          if (++count2 > 50) {
            ok(false, "Timed out while polling for the properties.");
            window.clearInterval(intervalID1);
            return resumeAndFinish();
          }
          if (!buttonProtoNode._retrieved ||
              !buttonAsProtoProtoNode._retrieved ||
              !documentProtoNode._retrieved) {
            return;
          }
          window.clearInterval(intervalID1);

          // Now the main course: make sure that the native getters for WebIDL
          // attributes have been called and a value has been returned.
          is(buttonProtoNode.get("type").target.querySelector(".name")
             .getAttribute("value"), "type",
            "Should have the right property name for 'type' in buttonProtoNode.");
          is(buttonProtoNode.get("type").target.querySelector(".value")
             .getAttribute("value"), '"submit"',
            "'type' in buttonProtoNode should have the right value.");
          is(buttonProtoNode.get("formMethod").target.querySelector(".name")
             .getAttribute("value"), "formMethod",
            "Should have the right property name for 'formMethod' in buttonProtoNode.");
          is(buttonProtoNode.get("formMethod").target.querySelector(".value")
             .getAttribute("value"), '""',
            "'formMethod' in buttonProtoNode should have the right value.");

          is(documentProtoNode.get("domain").target.querySelector(".name")
             .getAttribute("value"), "domain",
            "Should have the right property name for 'domain' in documentProtoNode.");
          is(documentProtoNode.get("domain").target.querySelector(".value")
             .getAttribute("value"), '"example.com"',
            "'domain' in documentProtoNode should have the right value.");
          is(documentProtoNode.get("cookie").target.querySelector(".name")
             .getAttribute("value"), "cookie",
            "Should have the right property name for 'cookie' in documentProtoNode.");
          is(documentProtoNode.get("cookie").target.querySelector(".value")
             .getAttribute("value"), '""',
            "'cookie' in documentProtoNode should have the right value.");

          let buttonAsProtoProtoProtoNode = buttonAsProtoProtoNode.get("__proto__");

          is(buttonAsProtoProtoProtoNode.expanded, false,
            "The buttonAsProtoProtoProtoNode should not be expanded at this point.");

          // Expand the prototype of the prototype of 'buttonAsProto' tree
          // node. This causes its properties to be retrieved and displayed.
          buttonAsProtoProtoProtoNode.expand();

          is(buttonAsProtoProtoProtoNode.expanded, true,
            "The buttonAsProtoProtoProtoNode should be expanded at this point.");

          // Poll every few milliseconds until the properties are retrieved.
          // It's important to set the timer in the chrome window, because the
          // content window timers are disabled while the debuggee is paused.
          let count3 = 0;
          let intervalID2 = window.setInterval(function(){
            info("count3: " + count3);
            if (++count3 > 50) {
              ok(false, "Timed out while polling for the properties.");
              window.clearInterval(intervalID2);
              return resumeAndFinish();
            }
            if (!buttonAsProtoProtoProtoNode._retrieved) {
              return;
            }
            window.clearInterval(intervalID2);

            // Test this more involved case that reuses an object that is
            // present in another cache line.
            is(buttonAsProtoProtoProtoNode.get("type").target.querySelector(".name")
               .getAttribute("value"), "type",
              "Should have the right property name for 'type' in buttonAsProtoProtoProtoNode.");
            is(buttonAsProtoProtoProtoNode.get("type").target.querySelector(".value")
               .getAttribute("value"), '"submit"',
              "'type' in buttonAsProtoProtoProtoNode should have the right value.");
            is(buttonAsProtoProtoProtoNode.get("formMethod").target.querySelector(".name")
               .getAttribute("value"), "formMethod",
              "Should have the right property name for 'formMethod' in buttonAsProtoProtoProtoNode.");
            is(buttonAsProtoProtoProtoNode.get("formMethod").target.querySelector(".value")
               .getAttribute("value"), '""',
              "'formMethod' in buttonAsProtoProtoProtoNode should have the right value.");

            resumeAndFinish();
          }, 100);
        }, 100);
      }, 100);
    }}, 0);
  }, false);

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function resumeAndFinish() {
  gDebugger.addEventListener("Debugger:AfterFramesCleared", function listener() {
    gDebugger.removeEventListener("Debugger:AfterFramesCleared", listener, true);
    Services.tm.currentThread.dispatch({ run: function() {
      let frames = gDebugger.DebuggerView.StackFrames._container._list;

      is(frames.querySelectorAll(".dbg-stackframe").length, 0,
        "Should have no frames.");

      closeDebuggerAndFinish();
    }}, 0);
  }, true);

  gDebugger.DebuggerController.activeThread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
