/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly displays WebIDL attributes in DOM
 * objects.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

var gTab, gPanel, gDebugger;
var gVariables;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 24)
      .then(expandGlobalScope)
      .then(performTest)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function expandGlobalScope() {
  let deferred = promise.defer();

  let globalScope = gVariables.getScopeAtIndex(2);
  is(globalScope.expanded, false,
    "The global scope should not be expanded by default.");

  gDebugger.once(gDebugger.EVENTS.FETCHED_VARIABLES, deferred.resolve);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    globalScope.target.querySelector(".name"),
    gDebugger);

  return deferred.promise;
}

function performTest() {
  let deferred = promise.defer();
  let globalScope = gVariables.getScopeAtIndex(2);

  let buttonVar = globalScope.get("button");
  let buttonAsProtoVar = globalScope.get("buttonAsProto");
  let documentVar = globalScope.get("document");

  is(buttonVar.target.querySelector(".name").getAttribute("value"), "button",
    "Should have the right property name for 'button'.");
  is(buttonVar.target.querySelector(".value").getAttribute("value"), "<button>",
    "Should have the right property value for 'button'.");
  ok(buttonVar.target.querySelector(".value").className.includes("token-domnode"),
    "Should have the right token class for 'button'.");

  is(buttonAsProtoVar.target.querySelector(".name").getAttribute("value"), "buttonAsProto",
    "Should have the right property name for 'buttonAsProto'.");
  is(buttonAsProtoVar.target.querySelector(".value").getAttribute("value"), "Object",
    "Should have the right property value for 'buttonAsProto'.");
  ok(buttonAsProtoVar.target.querySelector(".value").className.includes("token-other"),
    "Should have the right token class for 'buttonAsProto'.");

  is(documentVar.target.querySelector(".name").getAttribute("value"), "document",
    "Should have the right property name for 'document'.");
  is(documentVar.target.querySelector(".value").getAttribute("value"),
    "HTMLDocument \u2192 doc_frame-parameters.html",
    "Should have the right property value for 'document'.");
  ok(documentVar.target.querySelector(".value").className.includes("token-domnode"),
    "Should have the right token class for 'document'.");

  is(buttonVar.expanded, false,
    "The buttonVar should not be expanded at this point.");
  is(buttonAsProtoVar.expanded, false,
    "The buttonAsProtoVar should not be expanded at this point.");
  is(documentVar.expanded, false,
    "The documentVar should not be expanded at this point.");

  waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 3).then(() => {
    is(buttonVar.get("type").target.querySelector(".name").getAttribute("value"), "type",
      "Should have the right property name for 'type'.");
    is(buttonVar.get("type").target.querySelector(".value").getAttribute("value"), "\"submit\"",
      "Should have the right property value for 'type'.");
    ok(buttonVar.get("type").target.querySelector(".value").className.includes("token-string"),
      "Should have the right token class for 'type'.");

    is(buttonVar.get("childNodes").target.querySelector(".name").getAttribute("value"), "childNodes",
      "Should have the right property name for 'childNodes'.");
    is(buttonVar.get("childNodes").target.querySelector(".value").getAttribute("value"), "NodeList[1]",
      "Should have the right property value for 'childNodes'.");
    ok(buttonVar.get("childNodes").target.querySelector(".value").className.includes("token-other"),
      "Should have the right token class for 'childNodes'.");

    is(buttonVar.get("onclick").target.querySelector(".name").getAttribute("value"), "onclick",
      "Should have the right property name for 'onclick'.");
    is(buttonVar.get("onclick").target.querySelector(".value").getAttribute("value"), "onclick(event)",
      "Should have the right property value for 'onclick'.");
    ok(buttonVar.get("onclick").target.querySelector(".value").className.includes("token-other"),
      "Should have the right token class for 'onclick'.");

    is(documentVar.get("title").target.querySelector(".name").getAttribute("value"), "title",
      "Should have the right property name for 'title'.");
    is(documentVar.get("title").target.querySelector(".value").getAttribute("value"), "\"Debugger test page\"",
      "Should have the right property value for 'title'.");
    ok(documentVar.get("title").target.querySelector(".value").className.includes("token-string"),
      "Should have the right token class for 'title'.");

    is(documentVar.get("childNodes").target.querySelector(".name").getAttribute("value"), "childNodes",
      "Should have the right property name for 'childNodes'.");
    is(documentVar.get("childNodes").target.querySelector(".value").getAttribute("value"), "NodeList[3]",
      "Should have the right property value for 'childNodes'.");
    ok(documentVar.get("childNodes").target.querySelector(".value").className.includes("token-other"),
      "Should have the right token class for 'childNodes'.");

    is(documentVar.get("onclick").target.querySelector(".name").getAttribute("value"), "onclick",
      "Should have the right property name for 'onclick'.");
    is(documentVar.get("onclick").target.querySelector(".value").getAttribute("value"), "null",
      "Should have the right property value for 'onclick'.");
    ok(documentVar.get("onclick").target.querySelector(".value").className.includes("token-null"),
      "Should have the right token class for 'onclick'.");

    let buttonProtoVar = buttonVar.get("__proto__");
    let buttonAsProtoProtoVar = buttonAsProtoVar.get("__proto__");
    let documentProtoVar = documentVar.get("__proto__");

    is(buttonProtoVar.target.querySelector(".name").getAttribute("value"), "__proto__",
      "Should have the right property name for '__proto__'.");
    is(buttonProtoVar.target.querySelector(".value").getAttribute("value"), "HTMLButtonElementPrototype",
      "Should have the right property value for '__proto__'.");
    ok(buttonProtoVar.target.querySelector(".value").className.includes("token-other"),
      "Should have the right token class for '__proto__'.");

    is(buttonAsProtoProtoVar.target.querySelector(".name").getAttribute("value"), "__proto__",
      "Should have the right property name for '__proto__'.");
    is(buttonAsProtoProtoVar.target.querySelector(".value").getAttribute("value"), "<button>",
      "Should have the right property value for '__proto__'.");
    ok(buttonAsProtoProtoVar.target.querySelector(".value").className.includes("token-domnode"),
      "Should have the right token class for '__proto__'.");

    is(documentProtoVar.target.querySelector(".name").getAttribute("value"), "__proto__",
      "Should have the right property name for '__proto__'.");
    is(documentProtoVar.target.querySelector(".value").getAttribute("value"), "HTMLDocumentPrototype",
      "Should have the right property value for '__proto__'.");
    ok(documentProtoVar.target.querySelector(".value").className.includes("token-other"),
      "Should have the right token class for '__proto__'.");

    is(buttonProtoVar.expanded, false,
      "The buttonProtoVar should not be expanded at this point.");
    is(buttonAsProtoProtoVar.expanded, false,
      "The buttonAsProtoProtoVar should not be expanded at this point.");
    is(documentProtoVar.expanded, false,
      "The documentProtoVar should not be expanded at this point.");

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 3).then(() => {
      is(buttonAsProtoProtoVar.get("type").target.querySelector(".name").getAttribute("value"), "type",
        "Should have the right property name for 'type'.");
      is(buttonAsProtoProtoVar.get("type").target.querySelector(".value").getAttribute("value"), "\"submit\"",
        "Should have the right property value for 'type'.");
      ok(buttonAsProtoProtoVar.get("type").target.querySelector(".value").className.includes("token-string"),
        "Should have the right token class for 'type'.");

      is(buttonAsProtoProtoVar.get("childNodes").target.querySelector(".name").getAttribute("value"), "childNodes",
        "Should have the right property name for 'childNodes'.");
      is(buttonAsProtoProtoVar.get("childNodes").target.querySelector(".value").getAttribute("value"), "NodeList[1]",
        "Should have the right property value for 'childNodes'.");
      ok(buttonAsProtoProtoVar.get("childNodes").target.querySelector(".value").className.includes("token-other"),
        "Should have the right token class for 'childNodes'.");

      is(buttonAsProtoProtoVar.get("onclick").target.querySelector(".name").getAttribute("value"), "onclick",
        "Should have the right property name for 'onclick'.");
      is(buttonAsProtoProtoVar.get("onclick").target.querySelector(".value").getAttribute("value"), "onclick(event)",
        "Should have the right property value for 'onclick'.");
      ok(buttonAsProtoProtoVar.get("onclick").target.querySelector(".value").className.includes("token-other"),
        "Should have the right token class for 'onclick'.");

      let buttonProtoProtoVar = buttonProtoVar.get("__proto__");
      let buttonAsProtoProtoProtoVar = buttonAsProtoProtoVar.get("__proto__");
      let documentProtoProtoVar = documentProtoVar.get("__proto__");

      is(buttonProtoProtoVar.target.querySelector(".name").getAttribute("value"), "__proto__",
        "Should have the right property name for '__proto__'.");
      is(buttonProtoProtoVar.target.querySelector(".value").getAttribute("value"), "HTMLElementPrototype",
        "Should have the right property value for '__proto__'.");
      ok(buttonProtoProtoVar.target.querySelector(".value").className.includes("token-other"),
        "Should have the right token class for '__proto__'.");

      is(buttonAsProtoProtoProtoVar.target.querySelector(".name").getAttribute("value"), "__proto__",
        "Should have the right property name for '__proto__'.");
      is(buttonAsProtoProtoProtoVar.target.querySelector(".value").getAttribute("value"), "HTMLButtonElementPrototype",
        "Should have the right property value for '__proto__'.");
      ok(buttonAsProtoProtoProtoVar.target.querySelector(".value").className.includes("token-other"),
        "Should have the right token class for '__proto__'.");

      is(documentProtoProtoVar.target.querySelector(".name").getAttribute("value"), "__proto__",
        "Should have the right property name for '__proto__'.");
      is(documentProtoProtoVar.target.querySelector(".value").getAttribute("value"), "DocumentPrototype",
        "Should have the right property value for '__proto__'.");
      ok(documentProtoProtoVar.target.querySelector(".value").className.includes("token-other"),
        "Should have the right token class for '__proto__'.")

      is(buttonAsProtoProtoProtoVar.expanded, false,
        "The buttonAsProtoProtoProtoVar should not be expanded at this point.");
      is(buttonAsProtoProtoProtoVar.expanded, false,
        "The buttonAsProtoProtoProtoVar should not be expanded at this point.");
      is(documentProtoProtoVar.expanded, false,
        "The documentProtoProtoVar should not be expanded at this point.");

      deferred.resolve();
    });

    // Similarly, expand the 'button.__proto__', 'buttonAsProto.__proto__' and
    // 'document.__proto__' variables view nodes.
    buttonProtoVar.expand();
    buttonAsProtoProtoVar.expand();
    documentProtoVar.expand();

    is(buttonProtoVar.expanded, true,
      "The buttonProtoVar should be immediately marked as expanded.");
    is(buttonAsProtoProtoVar.expanded, true,
      "The buttonAsProtoProtoVar should be immediately marked as expanded.");
    is(documentProtoVar.expanded, true,
      "The documentProtoVar should be immediately marked as expanded.");
  });

  // Expand the 'button', 'buttonAsProto' and 'document' variables view nodes.
  // This causes their properties to be retrieved and displayed.
  buttonVar.expand();
  buttonAsProtoVar.expand();
  documentVar.expand();

  is(buttonVar.expanded, true,
    "The buttonVar should be immediately marked as expanded.");
  is(buttonAsProtoVar.expanded, true,
    "The buttonAsProtoVar should be immediately marked as expanded.");
  is(documentVar.expanded, true,
    "The documentVar should be immediately marked as expanded.");

  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
