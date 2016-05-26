/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "doc_closures.html";

// Test that inspecting a closure works as expected.

function test() {
  let gPanel, gTab, gDebugger;

  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    testClosure()
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function testClosure() {
    generateMouseClickInTab(gTab, "content.document.querySelector('button')");

    return waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
      let gVars = gDebugger.DebuggerView.Variables;
      let localScope = gVars.getScopeAtIndex(0);
      let localNodes = localScope.target.querySelector(".variables-view-element-details").childNodes;

      is(localNodes[4].querySelector(".name").getAttribute("value"), "person",
        "Should have the right property name for |person|.");
      is(localNodes[4].querySelector(".value").getAttribute("value"), "Object",
        "Should have the right property value for |person|.");

      // Expand the 'person' tree node. This causes its properties to be
      // retrieved and displayed.
      let personNode = gVars.getItemForNode(localNodes[4]);
      let personFetched = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES);
      personNode.expand();

      return personFetched.then(() => {
        is(personNode.expanded, true,
          "|person| should be expanded at this point.");

        is(personNode.get("getName").target.querySelector(".name")
           .getAttribute("value"), "getName",
          "Should have the right property name for 'getName' in person.");
        is(personNode.get("getName").target.querySelector(".value")
           .getAttribute("value"), "_pfactory/<.getName()",
          "'getName' in person should have the right value.");
        is(personNode.get("getFoo").target.querySelector(".name")
           .getAttribute("value"), "getFoo",
          "Should have the right property name for 'getFoo' in person.");
        is(personNode.get("getFoo").target.querySelector(".value")
           .getAttribute("value"), "_pfactory/<.getFoo()",
          "'getFoo' in person should have the right value.");

        // Expand the function nodes. This causes their properties to be
        // retrieved and displayed.
        let getFooNode = personNode.get("getFoo");
        let getNameNode = personNode.get("getName");
        let funcsFetched = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 2);
        let funcClosuresFetched = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES, 2);
        getFooNode.expand();
        getNameNode.expand();

        return funcsFetched.then(() => {
          is(getFooNode.expanded, true,
            "|person.getFoo| should be expanded at this point.");
          is(getNameNode.expanded, true,
            "|person.getName| should be expanded at this point.");

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

          // Expand the closure nodes. This causes their environments to be
          // retrieved and displayed.
          let getFooClosure = getFooNode.get("<Closure>");
          let getNameClosure = getNameNode.get("<Closure>");
          getFooClosure.expand();
          getNameClosure.expand();

          return funcClosuresFetched.then(() => {
            is(getFooClosure.expanded, true,
              "|person.getFoo| closure should be expanded at this point.");
            is(getNameClosure.expanded, true,
              "|person.getName| closure should be expanded at this point.");

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
            let innerFuncsFetched = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 2);
            getFooInnerScope.expand();
            getNameInnerScope.expand();

            return funcsFetched.then(() => {
              is(getFooInnerScope.expanded, true,
                "|person.getFoo| inner scope should be expanded at this point.");
              is(getNameInnerScope.expanded, true,
                "|person.getName| inner scope should be expanded at this point.");

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
            });
          });
        });
      });
    });
  }
}
