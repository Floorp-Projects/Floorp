/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that VariablesView methods responsible for styling variables
 * as overridden work properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_scope-variable-2.html";

function test() {
  Task.spawn(function* () {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let variables = win.DebuggerView.Variables;

    callInTab(tab, "test");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 23);

    let firstScope = variables.getScopeAtIndex(0);
    let secondScope = variables.getScopeAtIndex(1);
    let thirdScope = variables.getScopeAtIndex(2);
    let globalLexicalScope = variables.getScopeAtIndex(3);
    let globalScope = variables.getScopeAtIndex(4);

    ok(firstScope, "The first scope is available.");
    ok(secondScope, "The second scope is available.");
    ok(thirdScope, "The third scope is available.");
    ok(globalLexicalScope, "The global lexical scope is available.");
    ok(globalScope, "The global scope is available.");

    is(firstScope.name, "Function scope [secondNest]",
      "The first scope's name is correct.");
    is(secondScope.name, "Function scope [firstNest]",
      "The second scope's name is correct.");
    is(thirdScope.name, "Function scope [test]",
      "The third scope's name is correct.");
    is(globalLexicalScope.name, "Block scope",
      "The global lexical scope's name is correct.");
    is(globalScope.name, "Global scope [Window]",
      "The global scope's name is correct.");

    is(firstScope.expanded, true,
      "The first scope's expansion state is correct.");
    is(secondScope.expanded, false,
      "The second scope's expansion state is correct.");
    is(thirdScope.expanded, false,
      "The third scope's expansion state is correct.");
    is(globalLexicalScope.expanded, false,
      "The global lexical scope's expansion state is correct.");
    is(globalScope.expanded, false,
      "The global scope's expansion state is correct.");

    is(firstScope._store.size, 3,
      "The first scope should have all the variables available.");
    is(secondScope._store.size, 0,
      "The second scope should have no variables available yet.");
    is(thirdScope._store.size, 0,
      "The third scope should have no variables available yet.");
    is(globalLexicalScope._store.size, 0,
      "The global scope should have no variables available yet.");
    is(globalScope._store.size, 0,
      "The global scope should have no variables available yet.");

    // Test getOwnerScopeForVariableOrProperty with simple variables.

    let thisVar = firstScope.get("this");
    let thisOwner = variables.getOwnerScopeForVariableOrProperty(thisVar);
    is(thisOwner, firstScope,
      "The getOwnerScopeForVariableOrProperty method works properly (1).");

    let someVar1 = firstScope.get("a");
    let someOwner1 = variables.getOwnerScopeForVariableOrProperty(someVar1);
    is(someOwner1, firstScope,
      "The getOwnerScopeForVariableOrProperty method works properly (2).");

    // Test getOwnerScopeForVariableOrProperty with first-degree properties.

    let argsVar1 = firstScope.get("arguments");
    let fetched = waitForDebuggerEvents(panel, events.FETCHED_PROPERTIES);
    argsVar1.expand();
    yield fetched;

    let calleeProp1 = argsVar1.get("callee");
    let calleeOwner1 = variables.getOwnerScopeForVariableOrProperty(calleeProp1);
    is(calleeOwner1, firstScope,
      "The getOwnerScopeForVariableOrProperty method works properly (3).");

    // Test getOwnerScopeForVariableOrProperty with second-degree properties.

    let protoVar1 = argsVar1.get("__proto__");
    fetched = waitForDebuggerEvents(panel, events.FETCHED_PROPERTIES);
    protoVar1.expand();
    yield fetched;

    let constrProp1 = protoVar1.get("constructor");
    let constrOwner1 = variables.getOwnerScopeForVariableOrProperty(constrProp1);
    is(constrOwner1, firstScope,
      "The getOwnerScopeForVariableOrProperty method works properly (4).");

    // Test getOwnerScopeForVariableOrProperty with a simple variable
    // from non-topmost scopes.

    // Only need to wait for a single FETCHED_VARIABLES event, just for the
    // global scope, because the other local scopes already have the
    // arguments and variables available as evironment bindings.
    fetched = waitForDebuggerEvents(panel, events.FETCHED_VARIABLES);
    secondScope.expand();
    thirdScope.expand();
    globalLexicalScope.expand();
    globalScope.expand();
    yield fetched;

    let someVar2 = secondScope.get("a");
    let someOwner2 = variables.getOwnerScopeForVariableOrProperty(someVar2);
    is(someOwner2, secondScope,
      "The getOwnerScopeForVariableOrProperty method works properly (5).");

    let someVar3 = thirdScope.get("a");
    let someOwner3 = variables.getOwnerScopeForVariableOrProperty(someVar3);
    is(someOwner3, thirdScope,
      "The getOwnerScopeForVariableOrProperty method works properly (6).");

    // Test getOwnerScopeForVariableOrProperty with first-degree properies
    // from non-topmost scopes.

    let argsVar2 = secondScope.get("arguments");
    fetched = waitForDebuggerEvents(panel, events.FETCHED_PROPERTIES);
    argsVar2.expand();
    yield fetched;

    let calleeProp2 = argsVar2.get("callee");
    let calleeOwner2 = variables.getOwnerScopeForVariableOrProperty(calleeProp2);
    is(calleeOwner2, secondScope,
      "The getOwnerScopeForVariableOrProperty method works properly (7).");

    let argsVar3 = thirdScope.get("arguments");
    fetched = waitForDebuggerEvents(panel, events.FETCHED_PROPERTIES);
    argsVar3.expand();
    yield fetched;

    let calleeProp3 = argsVar3.get("callee");
    let calleeOwner3 = variables.getOwnerScopeForVariableOrProperty(calleeProp3);
    is(calleeOwner3, thirdScope,
      "The getOwnerScopeForVariableOrProperty method works properly (8).");

    // Test getOwnerScopeForVariableOrProperty with second-degree properties
    // from non-topmost scopes.

    let protoVar2 = argsVar2.get("__proto__");
    fetched = waitForDebuggerEvents(panel, events.FETCHED_PROPERTIES);
    protoVar2.expand();
    yield fetched;

    let constrProp2 = protoVar2.get("constructor");
    let constrOwner2 = variables.getOwnerScopeForVariableOrProperty(constrProp2);
    is(constrOwner2, secondScope,
      "The getOwnerScopeForVariableOrProperty method works properly (9).");

    let protoVar3 = argsVar3.get("__proto__");
    fetched = waitForDebuggerEvents(panel, events.FETCHED_PROPERTIES);
    protoVar3.expand();
    yield fetched;

    let constrProp3 = protoVar3.get("constructor");
    let constrOwner3 = variables.getOwnerScopeForVariableOrProperty(constrProp3);
    is(constrOwner3, thirdScope,
      "The getOwnerScopeForVariableOrProperty method works properly (10).");

    // Test getParentScopesForVariableOrProperty with simple variables.

    let varOwners1 = variables.getParentScopesForVariableOrProperty(someVar1);
    let varOwners2 = variables.getParentScopesForVariableOrProperty(someVar2);
    let varOwners3 = variables.getParentScopesForVariableOrProperty(someVar3);

    is(varOwners1.length, 0,
      "There should be no owner scopes for the first variable.");

    is(varOwners2.length, 1,
      "There should be one owner scope for the second variable.");
    is(varOwners2[0], firstScope,
      "The only owner scope for the second variable is correct.");

    is(varOwners3.length, 2,
      "There should be two owner scopes for the third variable.");
    is(varOwners3[0], firstScope,
      "The first owner scope for the third variable is correct.");
    is(varOwners3[1], secondScope,
      "The second owner scope for the third variable is correct.");

    // Test getParentScopesForVariableOrProperty with first-degree properties.

    let propOwners1 = variables.getParentScopesForVariableOrProperty(calleeProp1);
    let propOwners2 = variables.getParentScopesForVariableOrProperty(calleeProp2);
    let propOwners3 = variables.getParentScopesForVariableOrProperty(calleeProp3);

    is(propOwners1.length, 0,
      "There should be no owner scopes for the first property.");

    is(propOwners2.length, 1,
      "There should be one owner scope for the second property.");
    is(propOwners2[0], firstScope,
      "The only owner scope for the second property is correct.");

    is(propOwners3.length, 2,
      "There should be two owner scopes for the third property.");
    is(propOwners3[0], firstScope,
      "The first owner scope for the third property is correct.");
    is(propOwners3[1], secondScope,
      "The second owner scope for the third property is correct.");

    // Test getParentScopesForVariableOrProperty with second-degree properties.

    let secPropOwners1 = variables.getParentScopesForVariableOrProperty(constrProp1);
    let secPropOwners2 = variables.getParentScopesForVariableOrProperty(constrProp2);
    let secPropOwners3 = variables.getParentScopesForVariableOrProperty(constrProp3);

    is(secPropOwners1.length, 0,
      "There should be no owner scopes for the first inner property.");

    is(secPropOwners2.length, 1,
      "There should be one owner scope for the second inner property.");
    is(secPropOwners2[0], firstScope,
      "The only owner scope for the second inner property is correct.");

    is(secPropOwners3.length, 2,
      "There should be two owner scopes for the third inner property.");
    is(secPropOwners3[0], firstScope,
      "The first owner scope for the third inner property is correct.");
    is(secPropOwners3[1], secondScope,
      "The second owner scope for the third inner property is correct.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
