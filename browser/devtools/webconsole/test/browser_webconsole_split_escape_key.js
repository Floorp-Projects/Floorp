/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  info("Test various cases where the escape key should hide the split console.");

  let toolbox;
  let hud;
  let jsterm;
  let hudMessages;
  let variablesView;

  Task.spawn(runner).then(finish);

  function* runner() {
    let {tab} = yield loadTab("data:text/html;charset=utf-8,<p>Web Console test for splitting");
    let target = TargetFactory.forTab(tab);
    toolbox = yield gDevTools.showToolbox(target, "inspector");

    yield testCreateSplitConsoleAfterEscape();

    yield showAutoCompletePopoup();

    yield testHideAutoCompletePopupAfterEscape();

    yield executeJS();
    yield clickMessageAndShowVariablesView();
    jsterm.inputNode.focus();

    yield testHideVariablesViewAfterEscape();

    yield clickMessageAndShowVariablesView();
    yield startPropertyEditor();

    yield testCancelPropertyEditorAfterEscape();
    yield testHideVariablesViewAfterEscape();
    yield testHideSplitConsoleAfterEscape();
  }

  function testCreateSplitConsoleAfterEscape() {
    let result = toolbox.once("webconsole-ready", () => {
      hud = toolbox.getPanel("webconsole").hud;
      jsterm = hud.jsterm;
      ok(toolbox.splitConsole, "Split console is created.");
    });

    let contentWindow = toolbox.frame.contentWindow;
    contentWindow.focus();
    EventUtils.sendKey("ESCAPE", contentWindow);

    return result;
  }

  function testShowSplitConsoleAfterEscape() {
    let result = toolbox.once("split-console", () => {
      ok(toolbox.splitConsole, "Split console is shown.");
    });
    EventUtils.sendKey("ESCAPE", toolbox.frame.contentWindow);

    return result;
  }

  function testHideSplitConsoleAfterEscape() {
    let result = toolbox.once("split-console", () => {
      ok(!toolbox.splitConsole, "Split console is hidden.");
    });
    EventUtils.sendKey("ESCAPE", toolbox.frame.contentWindow);

    return result;
  }

  function testHideVariablesViewAfterEscape() {
    let result = jsterm.once("sidebar-closed", () => {
      ok(!hud.ui.jsterm.sidebar,
        "Variables view is hidden.");
      ok(toolbox.splitConsole,
        "Split console is open after hiding the variables view.");
    });
    EventUtils.sendKey("ESCAPE", toolbox.frame.contentWindow);

    return result;
  }

  function testHideAutoCompletePopupAfterEscape() {
    let deferred = promise.defer();
    let popup = jsterm.autocompletePopup;

    popup._panel.addEventListener("popuphidden", function popupHidden() {
      popup._panel.removeEventListener("popuphidden", popupHidden, false);
      ok(!popup.isOpen,
        "Auto complete popup is hidden.");
      ok(toolbox.splitConsole,
        "Split console is open after hiding the autocomplete popup.");

      deferred.resolve();
    }, false);

    EventUtils.sendKey("ESCAPE", toolbox.frame.contentWindow);

    return deferred.promise;
  }

  function testCancelPropertyEditorAfterEscape() {
    EventUtils.sendKey("ESCAPE", variablesView.window);
    ok(hud.ui.jsterm.sidebar,
      "Variables view is open after canceling property editor.");
    ok(toolbox.splitConsole,
      "Split console is open after editing.");
  }

  function* executeJS() {
    jsterm.execute("var foo = { bar: \"baz\" }; foo;");
    hudMessages = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "Object { bar: \"baz\" }",
        category: CATEGORY_OUTPUT,
        objects: true
      }],
    });
  }

  function clickMessageAndShowVariablesView() {
    let result = jsterm.once("variablesview-fetched", (event, vview) => {
      variablesView = vview;
    });

    let clickable = hudMessages[0].clickableElements[0];
    EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);

    return result;
  }

  function* startPropertyEditor() {
    let results = yield findVariableViewProperties(variablesView, [
      {name: "bar", value: "baz"}
    ], {webconsole: hud});
    results[0].matchedProp.focus();
    EventUtils.synthesizeKey("VK_RETURN", variablesView.window);
  }

  function showAutoCompletePopoup() {
    let deferred = promise.defer();
    let popupPanel = jsterm.autocompletePopup._panel;

    popupPanel.addEventListener("popupshown", function popupShown() {
      popupPanel.removeEventListener("popupshown", popupShown, false);
      deferred.resolve();
    }, false);

    jsterm.inputNode.focus();
    jsterm.setInputValue("document.location.");
    EventUtils.sendKey("TAB", hud.iframeWindow);

    return deferred.promise;
  }

  function finish() {
    toolbox.destroy().then(() => {
      toolbox = null;
      hud = null;
      jsterm = null;
      hudMessages = null;
      variablesView = null;

      finishTest();
    });
  }
}
