/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view correctly re-expands nodes after pauses.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_with-frame.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gDebuggee = null;

requestLongerTimeout(2);

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gDebuggee = aDebuggee;

    gDebugger.addEventListener("Debugger:SourceShown", function _onSourceShown() {
      gDebugger.removeEventListener("Debugger:SourceShown", _onSourceShown);
      addBreakpoint();
    });
  });
}

function addBreakpoint()
{
  gDebugger.DebuggerController.Breakpoints.addBreakpoint({
    url: gDebugger.DebuggerView.Sources.selectedValue,
    line: 16
  }, function(aBreakpointClient, aResponseError) {
    ok(!aResponseError, "There shouldn't be an error.");
    // Wait for the resume...
    gDebugger.gClient.addOneTimeListener("resumed", function() {
      gDebugger.DebuggerController.StackFrames.autoScopeExpand = true;
      gDebugger.DebuggerView.Variables.nonEnumVisible = false;
      gDebugger.DebuggerView.Variables.commitHierarchyIgnoredItems = Object.create(null);
      testVariablesExpand();
    });
  });
}

function testVariablesExpand()
{
  let count = 0;
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    // We expect 4 Debugger:FetchedVariables events, one from the global object
    // scope, two from the |with| scopes and the regular one.
    if (++count < 4) {
      info("Number of received Debugger:FetchedVariables events: " + count);
      return;
    }
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.StackFrames.widget._list,
          scopes = gDebugger.DebuggerView.Variables._list,
          innerScope = scopes.querySelectorAll(".variables-view-scope")[0],
          mathScope = scopes.querySelectorAll(".variables-view-scope")[1],
          testScope = scopes.querySelectorAll(".variables-view-scope")[2],
          loadScope = scopes.querySelectorAll(".variables-view-scope")[3],
          globalScope = scopes.querySelectorAll(".variables-view-scope")[4];

      let innerScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        innerScope.querySelector(".name").getAttribute("value"));
      let mathScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        mathScope.querySelector(".name").getAttribute("value"));
      let testScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        testScope.querySelector(".name").getAttribute("value"));
      let loadScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        loadScope.querySelector(".name").getAttribute("value"));
      let globalScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        globalScope.querySelector(".name").getAttribute("value"));

      is(innerScope.querySelector(".arrow").hasAttribute("open"), true,
        "The innerScope arrow should initially be expanded");
      is(mathScope.querySelector(".arrow").hasAttribute("open"), true,
        "The mathScope arrow should initially be expanded");
      is(testScope.querySelector(".arrow").hasAttribute("open"), true,
        "The testScope arrow should initially be expanded");
      is(loadScope.querySelector(".arrow").hasAttribute("open"), true,
        "The loadScope arrow should initially be expanded");
      is(globalScope.querySelector(".arrow").hasAttribute("open"), true,
        "The globalScope arrow should initially be expanded");

      is(innerScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The innerScope enumerables should initially be expanded");
      is(mathScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The mathScope enumerables should initially be expanded");
      is(testScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The testScope enumerables should initially be expanded");
      is(loadScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The loadScope enumerables should initially be expanded");
      is(globalScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The globalScope enumerables should initially be expanded");

      is(innerScopeItem.expanded, true,
        "The innerScope expanded getter should return true");
      is(mathScopeItem.expanded, true,
        "The mathScope expanded getter should return true");
      is(testScopeItem.expanded, true,
        "The testScope expanded getter should return true");
      is(loadScopeItem.expanded, true,
        "The loadScope expanded getter should return true");
      is(globalScopeItem.expanded, true,
        "The globalScope expanded getter should return true");

      mathScopeItem.collapse();
      testScopeItem.collapse();
      loadScopeItem.collapse();
      globalScopeItem.collapse();

      is(innerScope.querySelector(".arrow").hasAttribute("open"), true,
        "The innerScope arrow should initially be expanded");
      is(mathScope.querySelector(".arrow").hasAttribute("open"), false,
        "The mathScope arrow should initially not be expanded");
      is(testScope.querySelector(".arrow").hasAttribute("open"), false,
        "The testScope arrow should initially not be expanded");
      is(loadScope.querySelector(".arrow").hasAttribute("open"), false,
        "The loadScope arrow should initially not be expanded");
      is(globalScope.querySelector(".arrow").hasAttribute("open"), false,
        "The globalScope arrow should initially not be expanded");

      is(innerScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The innerScope enumerables should initially be expanded");
      is(mathScope.querySelector(".variables-view-element-details").hasAttribute("open"), false,
        "The mathScope enumerables should initially not be expanded");
      is(testScope.querySelector(".variables-view-element-details").hasAttribute("open"), false,
        "The testScope enumerables should initially not be expanded");
      is(loadScope.querySelector(".variables-view-element-details").hasAttribute("open"), false,
        "The loadScope enumerables should initially not be expanded");
      is(globalScope.querySelector(".variables-view-element-details").hasAttribute("open"), false,
        "The globalScope enumerables should initially not be expanded");

      is(innerScopeItem.expanded, true,
        "The innerScope expanded getter should return true");
      is(mathScopeItem.expanded, false,
        "The mathScope expanded getter should return false");
      is(testScopeItem.expanded, false,
        "The testScope expanded getter should return false");
      is(loadScopeItem.expanded, false,
        "The loadScope expanded getter should return false");
      is(globalScopeItem.expanded, false,
        "The globalScope expanded getter should return false");


      EventUtils.sendMouseEvent({ type: "mousedown" }, mathScope.querySelector(".arrow"), gDebugger);
      EventUtils.sendMouseEvent({ type: "mousedown" }, testScope.querySelector(".arrow"), gDebugger);
      EventUtils.sendMouseEvent({ type: "mousedown" }, loadScope.querySelector(".arrow"), gDebugger);
      EventUtils.sendMouseEvent({ type: "mousedown" }, globalScope.querySelector(".arrow"), gDebugger);


      is(innerScope.querySelector(".arrow").hasAttribute("open"), true,
        "The innerScope arrow should now be expanded");
      is(mathScope.querySelector(".arrow").hasAttribute("open"), true,
        "The mathScope arrow should now be expanded");
      is(testScope.querySelector(".arrow").hasAttribute("open"), true,
        "The testScope arrow should now be expanded");
      is(loadScope.querySelector(".arrow").hasAttribute("open"), true,
        "The loadScope arrow should now be expanded");
      is(globalScope.querySelector(".arrow").hasAttribute("open"), true,
        "The globalScope arrow should now be expanded");

      is(innerScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The innerScope enumerables should now be expanded");
      is(mathScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The mathScope enumerables should now be expanded");
      is(testScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The testScope enumerables should now be expanded");
      is(loadScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The loadScope enumerables should now be expanded");
      is(globalScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
        "The globalScope enumerables should now be expanded");

      is(innerScopeItem.expanded, true,
        "The innerScope expanded getter should return true");
      is(mathScopeItem.expanded, true,
        "The mathScope expanded getter should return true");
      is(testScopeItem.expanded, true,
        "The testScope expanded getter should return true");
      is(loadScopeItem.expanded, true,
        "The loadScope expanded getter should return true");
      is(globalScopeItem.expanded, true,
        "The globalScope expanded getter should return true");


      let thisItem = innerScopeItem.get("this");
      is(thisItem.expanded, false,
        "The local scope 'this' should not be expanded yet");

      gDebugger.addEventListener("Debugger:FetchedProperties", function test2() {
        gDebugger.removeEventListener("Debugger:FetchedProperties", test2, false);
        Services.tm.currentThread.dispatch({ run: function() {

          let windowItem = thisItem.get("window");
          is(windowItem.expanded, false,
            "The local scope 'this.window' should not be expanded yet");

          gDebugger.addEventListener("Debugger:FetchedProperties", function test3() {
            gDebugger.removeEventListener("Debugger:FetchedProperties", test3, false);
            Services.tm.currentThread.dispatch({ run: function() {

              let documentItem = windowItem.get("document");
              is(documentItem.expanded, false,
                "The local scope 'this.window.document' should not be expanded yet");

              gDebugger.addEventListener("Debugger:FetchedProperties", function test4() {
                gDebugger.removeEventListener("Debugger:FetchedProperties", test4, false);
                Services.tm.currentThread.dispatch({ run: function() {

                  let locationItem = documentItem.get("location");
                  is(locationItem.expanded, false,
                    "The local scope 'this.window.document.location' should not be expanded yet");

                  gDebugger.addEventListener("Debugger:FetchedProperties", function test5() {
                    gDebugger.removeEventListener("Debugger:FetchedProperties", test5, false);
                    Services.tm.currentThread.dispatch({ run: function() {

                      is(thisItem.target.querySelector(".arrow").hasAttribute("open"), true,
                        "The thisItem arrow should still be expanded (1)");
                      is(windowItem.target.querySelector(".arrow").hasAttribute("open"), true,
                        "The windowItem arrow should still be expanded (1)");
                      is(documentItem.target.querySelector(".arrow").hasAttribute("open"), true,
                        "The documentItem arrow should still be expanded (1)");
                      is(locationItem.target.querySelector(".arrow").hasAttribute("open"), true,
                        "The locationItem arrow should still be expanded (1)");

                      is(thisItem.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                        "The thisItem enumerables should still be expanded (1)");
                      is(windowItem.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                        "The windowItem enumerables should still be expanded (1)");
                      is(documentItem.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                        "The documentItem enumerables should still be expanded (1)");
                      is(locationItem.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                        "The locationItem enumerables should still be expanded (1)");

                      is(thisItem.expanded, true,
                        "The local scope 'this' should still be expanded (1)");
                      is(windowItem.expanded, true,
                        "The local scope 'this.window' should still be expanded (1)");
                      is(documentItem.expanded, true,
                        "The local scope 'this.window.document' should still be expanded (1)");
                      is(locationItem.expanded, true,
                        "The local scope 'this.window.document.location' should still be expanded (1)");


                      let count = 0;
                      gDebugger.addEventListener("Debugger:FetchedProperties", function test6() {
                        // We expect 4 Debugger:FetchedProperties events, one from the this
                        // reference, one for window, one for document and one for location.
                        if (++count < 4) {
                          info("Number of received Debugger:FetchedProperties events: " + count);
                          return;
                        }
                        gDebugger.removeEventListener("Debugger:FetchedProperties", test6, false);
                        Services.tm.currentThread.dispatch({ run: function() {

                          is(innerScope.querySelector(".arrow").hasAttribute("open"), true,
                            "The innerScope arrow should still be expanded");
                          is(mathScope.querySelector(".arrow").hasAttribute("open"), true,
                            "The mathScope arrow should still be expanded");
                          is(testScope.querySelector(".arrow").hasAttribute("open"), true,
                            "The testScope arrow should still be expanded");
                          is(loadScope.querySelector(".arrow").hasAttribute("open"), true,
                            "The loadScope arrow should still be expanded");
                          is(globalScope.querySelector(".arrow").hasAttribute("open"), true,
                            "The globalScope arrow should still be expanded");

                          is(innerScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The innerScope enumerables should still be expanded");
                          is(mathScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The mathScope enumerables should still be expanded");
                          is(testScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The testScope enumerables should still be expanded");
                          is(loadScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The loadScope enumerables should still be expanded");
                          is(globalScope.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The globalScope enumerables should still be expanded");

                          is(innerScopeItem.expanded, true,
                            "The innerScope expanded getter should return true");
                          is(mathScopeItem.expanded, true,
                            "The mathScope expanded getter should return true");
                          is(testScopeItem.expanded, true,
                            "The testScope expanded getter should return true");
                          is(loadScopeItem.expanded, true,
                            "The loadScope expanded getter should return true");
                          is(globalScopeItem.expanded, true,
                            "The globalScope expanded getter should return true");

                          is(thisItem.target.querySelector(".arrow").hasAttribute("open"), true,
                            "The thisItem arrow should still be expanded (2)");
                          is(windowItem.target.querySelector(".arrow").hasAttribute("open"), true,
                            "The windowItem arrow should still be expanded (2)");
                          is(documentItem.target.querySelector(".arrow").hasAttribute("open"), true,
                            "The documentItem arrow should still be expanded (2)");
                          is(locationItem.target.querySelector(".arrow").hasAttribute("open"), true,
                            "The locationItem arrow should still be expanded (2)");

                          is(thisItem.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The thisItem enumerables should still be expanded (2)");
                          is(windowItem.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The windowItem enumerables should still be expanded (2)");
                          is(documentItem.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The documentItem enumerables should still be expanded (2)");
                          is(locationItem.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
                            "The locationItem enumerables should still be expanded (2)");

                          is(thisItem.expanded, true,
                            "The local scope 'this' should still be expanded (2)");
                          is(windowItem.expanded, true,
                            "The local scope 'this.window' should still be expanded (2)");
                          is(documentItem.expanded, true,
                            "The local scope 'this.window.document' should still be expanded (2)");
                          is(locationItem.expanded, true,
                            "The local scope 'this.window.document.location' should still be expanded (2)");

                          executeSoon(function() {
                            closeDebuggerAndFinish();
                          });
                        }}, 0);
                      }, false);

                      executeSoon(function() {
                        EventUtils.sendMouseEvent({ type: "mousedown" },
                          gDebugger.document.querySelector("#step-in"),
                          gDebugger);
                      });
                    }}, 0);
                  }, false);

                  executeSoon(function() {
                    EventUtils.sendMouseEvent({ type: "mousedown" },
                      locationItem.target.querySelector(".arrow"),
                      gDebugger);

                    is(locationItem.expanded, true,
                      "The local scope 'this.window.document.location' should be expanded now");
                  });
                }}, 0);
              }, false);

              executeSoon(function() {
                EventUtils.sendMouseEvent({ type: "mousedown" },
                  documentItem.target.querySelector(".arrow"),
                  gDebugger);

                is(documentItem.expanded, true,
                  "The local scope 'this.window.document' should be expanded now");
              });
            }}, 0);
          }, false);

          executeSoon(function() {
            EventUtils.sendMouseEvent({ type: "mousedown" },
              windowItem.target.querySelector(".arrow"),
              gDebugger);

            is(windowItem.expanded, true,
              "The local scope 'this.window' should be expanded now");
          });
        }}, 0);
      }, false);

      executeSoon(function() {
        EventUtils.sendMouseEvent({ type: "mousedown" },
          thisItem.target.querySelector(".arrow"),
          gDebugger);

        is(thisItem.expanded, true,
          "The local scope 'this' should be expanded now");
      });
    }}, 0);
  }, false);

  EventUtils.sendMouseEvent({ type: "click" },
    gDebuggee.document.querySelector("button"),
    gDebuggee.window);
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gDebuggee = null;
});
