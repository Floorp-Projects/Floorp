/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TAB_URL = EXAMPLE_URL + "doc_closures.html";

// Test that inspecting a closure works as expected.

function test() {
  let gPanel, gTab, gDebuggee, gDebugger;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    waitForSourceShown(gPanel, ".html")
      .then(testClosure)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function testClosure() {
    // Spin the event loop before causing the debuggee to pause, to allow
    // this function to return first.
    executeSoon(() => {
      EventUtils.sendMouseEvent({ type: "click" },
        gDebuggee.document.querySelector("button"),
        gDebuggee);
    });

    gDebuggee.gRecurseLimit = 2;

    return waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
      let deferred = promise.defer();

      let gVars = gDebugger.DebuggerView.Variables,
          localScope = gVars.getScopeAtIndex(0),
          globalScope = gVars.getScopeAtIndex(1),
          localNodes = localScope.target.querySelector(".variables-view-element-details").childNodes,
          globalNodes = globalScope.target.querySelector(".variables-view-element-details").childNodes;

      is(localNodes[4].querySelector(".name").getAttribute("value"), "person",
        "Should have the right property name for |person|.");

      is(localNodes[4].querySelector(".value").getAttribute("value"), "Object",
        "Should have the right property value for |person|.");

      // Expand the 'person' tree node. This causes its properties to be
      // retrieved and displayed.
      let personNode = gVars.getItemForNode(localNodes[4]);
      personNode.expand();
      is(personNode.expanded, true, "person should be expanded at this point.");

      // Poll every few milliseconds until the properties are retrieved.
      // It's important to set the timer in the chrome window, because the
      // content window timers are disabled while the debuggee is paused.
      let count1 = 0;
      let intervalID = window.setInterval(function(){
        info("count1: " + count1);
        if (++count1 > 50) {
          ok(false, "Timed out while polling for the properties.");
          window.clearInterval(intervalID);
          deferred.reject("Timed out.");
          return;
        }
        if (!personNode._retrieved) {
          return;
        }
        window.clearInterval(intervalID);

        is(personNode.get("getName").target.querySelector(".name")
           .getAttribute("value"), "getName",
          "Should have the right property name for 'getName' in person.");
        is(personNode.get("getName").target.querySelector(".value")
           .getAttribute("value"), "Function",
          "'getName' in person should have the right value.");
        is(personNode.get("getFoo").target.querySelector(".name")
           .getAttribute("value"), "getFoo",
          "Should have the right property name for 'getFoo' in person.");
        is(personNode.get("getFoo").target.querySelector(".value")
           .getAttribute("value"), "Function",
          "'getFoo' in person should have the right value.");

        // Expand the function nodes. This causes their properties to be
        // retrieved and displayed.
        let getFooNode = personNode.get("getFoo");
        let getNameNode = personNode.get("getName");
        getFooNode.expand();
        getNameNode.expand();
        is(getFooNode.expanded, true, "person.getFoo should be expanded at this point.");
        is(getNameNode.expanded, true, "person.getName should be expanded at this point.");

        // Poll every few milliseconds until the properties are retrieved.
        // It's important to set the timer in the chrome window, because the
        // content window timers are disabled while the debuggee is paused.
        let count2 = 0;
        let intervalID1 = window.setInterval(function(){
          info("count2: " + count2);
          if (++count2 > 50) {
            ok(false, "Timed out while polling for the properties.");
            window.clearInterval(intervalID1);
            deferred.reject("Timed out.");
            return;
          }
          if (!getFooNode._retrieved || !getNameNode._retrieved) {
            return;
          }
          window.clearInterval(intervalID1);

          is(getFooNode.get("<Closure>").target.querySelector(".name")
             .getAttribute("value"), "<Closure>",
            "Found the closure node for getFoo.");
          is(getFooNode.get("<Closure>").target.querySelector(".value")
             .getAttribute("value"), "",
            "The closure node has no value for getFoo.");
          is(getNameNode.get("<Closure>").target.querySelector(".name")
             .getAttribute("value"), "<Closure>",
            "Found the closure node for getName.");
          is(getNameNode.get("<Closure>").target.querySelector(".value")
             .getAttribute("value"), "",
            "The closure node has no value for getName.");

          // Expand the Closure nodes.
          let getFooClosure = getFooNode.get("<Closure>");
          let getNameClosure = getNameNode.get("<Closure>");
          getFooClosure.expand();
          getNameClosure.expand();
          is(getFooClosure.expanded, true, "person.getFoo closure should be expanded at this point.");
          is(getNameClosure.expanded, true, "person.getName closure should be expanded at this point.");

          // Poll every few milliseconds until the properties are retrieved.
          // It's important to set the timer in the chrome window, because the
          // content window timers are disabled while the debuggee is paused.
          let count3 = 0;
          let intervalID2 = window.setInterval(function(){
            info("count3: " + count3);
            if (++count3 > 50) {
              ok(false, "Timed out while polling for the properties.");
              window.clearInterval(intervalID2);
              deferred.reject("Timed out.");
              return;
            }
            if (!getFooClosure._retrieved || !getNameClosure._retrieved) {
              return;
            }
            window.clearInterval(intervalID2);

            is(getFooClosure.get("Function scope [_pfactory]").target.querySelector(".name")
               .getAttribute("value"), "Function scope [_pfactory]",
              "Found the function scope node for the getFoo closure.");
            is(getFooClosure.get("Function scope [_pfactory]").target.querySelector(".value")
               .getAttribute("value"), "",
              "The function scope node has no value for the getFoo closure.");
            is(getNameClosure.get("Function scope [_pfactory]").target.querySelector(".name")
               .getAttribute("value"), "Function scope [_pfactory]",
              "Found the function scope node for the getName closure.");
            is(getNameClosure.get("Function scope [_pfactory]").target.querySelector(".value")
               .getAttribute("value"), "",
              "The function scope node has no value for the getName closure.");

            // Expand the scope nodes.
            let getFooInnerScope = getFooClosure.get("Function scope [_pfactory]");
            let getNameInnerScope = getNameClosure.get("Function scope [_pfactory]");
            getFooInnerScope.expand();
            getNameInnerScope.expand();
            is(getFooInnerScope.expanded, true, "person.getFoo inner scope should be expanded at this point.");
            is(getNameInnerScope.expanded, true, "person.getName inner scope should be expanded at this point.");

            // Poll every few milliseconds until the properties are retrieved.
            // It's important to set the timer in the chrome window, because the
            // content window timers are disabled while the debuggee is paused.
            let count4 = 0;
            let intervalID3 = window.setInterval(function(){
              info("count4: " + count4);
              if (++count4 > 50) {
                ok(false, "Timed out while polling for the properties.");
                window.clearInterval(intervalID3);
                deferred.reject("Timed out.");
                return;
              }
              if (!getFooInnerScope._retrieved || !getNameInnerScope._retrieved) {
                return;
              }
              window.clearInterval(intervalID3);

              // Only test that each function closes over the necessary variable.
              // We wouldn't want future SpiderMonkey closure space
              // optimizations to break this test.
              is(getFooInnerScope.get("foo").target.querySelector(".name")
                 .getAttribute("value"), "foo",
                "Found the foo node for the getFoo inner scope.");
              is(getFooInnerScope.get("foo").target.querySelector(".value")
                 .getAttribute("value"), "10",
                "The foo node has the expected value.");
              is(getNameInnerScope.get("name").target.querySelector(".name")
                 .getAttribute("value"), "name",
                "Found the name node for the getName inner scope.");
              is(getNameInnerScope.get("name").target.querySelector(".value")
                 .getAttribute("value"), '"Bob"',
                "The name node has the expected value.");

              deferred.resolve();
            }, 100);
          }, 100);
        }, 100);
      }, 100);

      return deferred.promise;
    });
  }
}
