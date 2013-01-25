/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view correctly filters nodes by name.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_with-frame.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gDebuggee = null;
var gSearchBox = null;

requestLongerTimeout(2);

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gDebuggee = aDebuggee;

    gDebugger.DebuggerController.StackFrames.autoScopeExpand = true;
    gDebugger.DebuggerView.Variables.delayedSearch = false;
    testSearchbox();
    prepareVariables(testVariablesFiltering);
  });
}

function testSearchbox()
{
  ok(!gDebugger.DebuggerView.Variables._searchboxNode,
    "There should not initially be a searchbox available in the variables view.");
  ok(!gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "The searchbox element should not be found.");

  gDebugger.DebuggerView.Variables._enableSearch();
  ok(gDebugger.DebuggerView.Variables._searchboxNode,
    "There should be a searchbox available after enabling.");
  ok(gDebugger.DebuggerView.Variables._searchboxContainer.hidden,
    "The searchbox container should be hidden at this point.");
  ok(gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "The searchbox element should be found.");


  gDebugger.DebuggerView.Variables._disableSearch();
  ok(!gDebugger.DebuggerView.Variables._searchboxNode,
    "There shouldn't be a searchbox available after disabling.");
  ok(!gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "The searchbox element should not be found.");

  gDebugger.DebuggerView.Variables._enableSearch();
  ok(gDebugger.DebuggerView.Variables._searchboxNode,
    "There should be a searchbox available after enabling.");
  ok(gDebugger.DebuggerView.Variables._searchboxContainer.hidden,
    "The searchbox container should be hidden at this point.");
  ok(gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "The searchbox element should be found.");


  let placeholder = "freshly squeezed mango juice";

  gDebugger.DebuggerView.Variables.searchPlaceholder = placeholder;
  is(gDebugger.DebuggerView.Variables.searchPlaceholder, placeholder,
    "The placeholder getter didn't return the expected string");

  ok(gDebugger.DebuggerView.Variables._searchboxNode.getAttribute("placeholder"),
    placeholder, "There correct placeholder should be applied to the searchbox.");


  gDebugger.DebuggerView.Variables._disableSearch();
  ok(!gDebugger.DebuggerView.Variables._searchboxNode,
    "There shouldn't be a searchbox available after disabling again.");
  ok(!gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "The searchbox element should not be found.");

  gDebugger.DebuggerView.Variables._enableSearch();
  ok(gDebugger.DebuggerView.Variables._searchboxNode,
    "There should be a searchbox available after enabling again.");
  ok(gDebugger.DebuggerView.Variables._searchboxContainer.hidden,
    "The searchbox container should be hidden at this point.");
  ok(gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "The searchbox element should be found.");

  ok(gDebugger.DebuggerView.Variables._searchboxNode.getAttribute("placeholder"),
    placeholder, "There correct placeholder should be applied to the searchbox again.");


  gDebugger.DebuggerView.Variables.searchEnabled = false;
  ok(!gDebugger.DebuggerView.Variables._searchboxNode,
    "There shouldn't be a searchbox available after disabling again.");
  ok(!gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "The searchbox element should not be found.");

  gDebugger.DebuggerView.Variables.searchEnabled = true;
  ok(gDebugger.DebuggerView.Variables._searchboxNode,
    "There should be a searchbox available after enabling again.");
  ok(gDebugger.DebuggerView.Variables._searchboxContainer.hidden,
    "The searchbox container should be hidden at this point.");
  ok(gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "The searchbox element should be found.");

  ok(gDebugger.DebuggerView.Variables._searchboxNode.getAttribute("placeholder"),
    placeholder, "There correct placeholder should be applied to the searchbox again.");
}

function testVariablesFiltering()
{
  ok(!gDebugger.DebuggerView.Variables._searchboxContainer.hidden,
    "The searchbox container should not be hidden at this point.");

  function test1()
  {
    write("location");

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

    is(thisItem.expanded, true,
      "The local scope 'this' should be expanded");
    is(windowItem.expanded, true,
      "The local scope 'this.window' should be expanded");
    is(documentItem.expanded, true,
      "The local scope 'this.window.document' should be expanded");
    is(locationItem.expanded, true,
      "The local scope 'this.window.document.location' should be expanded");

    ignoreExtraMatchedProperties();
    locationItem.toggle();
    locationItem.toggle();

    is(innerScope.querySelectorAll(".variable:not([non-match])").length, 1,
      "There should be 1 variable displayed in the inner scope");
    is(mathScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the math scope");
    is(testScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the test scope");
    is(loadScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the load scope");
    is(globalScope.querySelectorAll(".variable:not([non-match])").length, 1,
      "There should be 1 variable displayed in the global scope");

    is(innerScope.querySelectorAll(".property:not([non-match])").length, 6,
      "There should be 6 properties displayed in the inner scope");
    is(mathScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the math scope");
    is(testScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the test scope");
    is(loadScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the load scope");
    is(globalScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the global scope");

    is(innerScope.querySelectorAll(".variable:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "this", "The only inner variable displayed should be 'this'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "window", "The first inner property displayed should be 'window'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[1].getAttribute("value"),
      "document", "The second inner property displayed should be 'document'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[2].getAttribute("value"),
      "location", "The third inner property displayed should be 'location'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[3].getAttribute("value"),
      "__proto__", "The fourth inner property displayed should be '__proto__'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[4].getAttribute("value"),
      "Location", "The fifth inner property displayed should be 'Location'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[5].getAttribute("value"),
      "Location", "The sixth inner property displayed should be 'Location'");

    is(globalScope.querySelectorAll(".variable:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "Location", "The only global variable displayed should be 'Location'");
  }

  function test2()
  {
    innerScopeItem.collapse();
    mathScopeItem.collapse();
    testScopeItem.collapse();
    loadScopeItem.collapse();
    globalScopeItem.collapse();
    thisItem.collapse();
    windowItem.collapse();
    documentItem.collapse();
    locationItem.collapse();

    is(innerScopeItem.expanded, false,
      "The innerScope expanded getter should return false");
    is(mathScopeItem.expanded, false,
      "The mathScope expanded getter should return false");
    is(testScopeItem.expanded, false,
      "The testScope expanded getter should return false");
    is(loadScopeItem.expanded, false,
      "The loadScope expanded getter should return false");
    is(globalScopeItem.expanded, false,
      "The globalScope expanded getter should return false");

    is(thisItem.expanded, false,
      "The local scope 'this' should not be expanded");
    is(windowItem.expanded, false,
      "The local scope 'this.window' should not be expanded");
    is(documentItem.expanded, false,
      "The local scope 'this.window.document' should not be expanded");
    is(locationItem.expanded, false,
      "The local scope 'this.window.document.location' should not be expanded");

    write("location");

    is(thisItem.expanded, true,
      "The local scope 'this' should be expanded");
    is(windowItem.expanded, true,
      "The local scope 'this.window' should be expanded");
    is(documentItem.expanded, true,
      "The local scope 'this.window.document' should be expanded");
    is(locationItem.expanded, true,
      "The local scope 'this.window.document.location' should be expanded");

    ignoreExtraMatchedProperties();
    locationItem.toggle();
    locationItem.toggle();

    is(innerScope.querySelectorAll(".variable:not([non-match])").length, 1,
      "There should be 1 variable displayed in the inner scope");
    is(mathScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the math scope");
    is(testScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the test scope");
    is(loadScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the load scope");
    is(globalScope.querySelectorAll(".variable:not([non-match])").length, 1,
      "There should be 1 variable displayed in the global scope");

    is(innerScope.querySelectorAll(".property:not([non-match])").length, 6,
      "There should be 6 properties displayed in the inner scope");
    is(mathScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the math scope");
    is(testScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the test scope");
    is(loadScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the load scope");
    is(globalScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the global scope");

    is(innerScope.querySelectorAll(".variable:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "this", "The only inner variable displayed should be 'this'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "window", "The first inner property displayed should be 'window'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[1].getAttribute("value"),
      "document", "The second inner property displayed should be 'document'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[2].getAttribute("value"),
      "location", "The third inner property displayed should be 'location'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[3].getAttribute("value"),
      "__proto__", "The fourth inner property displayed should be '__proto__'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[4].getAttribute("value"),
      "Location", "The fifth inner property displayed should be 'Location'");
    is(innerScope.querySelectorAll(".property:not([non-match]) > .title > .name")[5].getAttribute("value"),
      "Location", "The sixth inner property displayed should be 'Location'");

    is(globalScope.querySelectorAll(".variable:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "Location", "The only global variable displayed should be 'Location'");
  }

  var scopes = gDebugger.DebuggerView.Variables._list,
      innerScope = scopes.querySelectorAll(".scope")[0],
      mathScope = scopes.querySelectorAll(".scope")[1],
      testScope = scopes.querySelectorAll(".scope")[2],
      loadScope = scopes.querySelectorAll(".scope")[3],
      globalScope = scopes.querySelectorAll(".scope")[4];

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

  let thisItem = innerScopeItem.get("this");
  let windowItem = thisItem.get("window");
  let documentItem = windowItem.get("document");
  let locationItem = documentItem.get("location");

  gSearchBox = gDebugger.DebuggerView.Variables._searchboxNode;

  executeSoon(function() {
    test1();
    executeSoon(function() {
      test2();
      executeSoon(function() {
        closeDebuggerAndFinish();
      });
    });
  });
}

function prepareVariables(aCallback)
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

      var frames = gDebugger.DebuggerView.StackFrames._container._list,
          scopes = gDebugger.DebuggerView.Variables._list,
          innerScope = scopes.querySelectorAll(".scope")[0],
          mathScope = scopes.querySelectorAll(".scope")[1],
          testScope = scopes.querySelectorAll(".scope")[2],
          loadScope = scopes.querySelectorAll(".scope")[3],
          globalScope = scopes.querySelectorAll(".scope")[4];

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

                      is(thisItem.expanded, true,
                        "The local scope 'this' should be expanded");
                      is(windowItem.expanded, true,
                        "The local scope 'this.window' should be expanded");
                      is(documentItem.expanded, true,
                        "The local scope 'this.window.document' should be expanded");
                      is(locationItem.expanded, true,
                        "The local scope 'this.window.document.location' should be expanded");

                      executeSoon(function() {
                        aCallback();
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

function ignoreExtraMatchedProperties()
{
  for (let [, item] of gDebugger.DebuggerView.Variables._currHierarchy) {
    let name = item.name.toLowerCase();
    let value = item._valueString || "";

    if ((name.contains("tracemallocdumpallocations")) ||
        (name.contains("geolocation")) ||
        (name.contains("webgl"))) {
      item.target.setAttribute("non-match", "");
    }
  }
}

function clear() {
  gSearchBox.focus();
  gSearchBox.value = "";
}

function write(text) {
  clear();
  append(text);
}

function append(text) {
  gSearchBox.focus();

  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i], gDebugger);
  }
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gDebuggee = null;
  gSearchBox = null;
});
