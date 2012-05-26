/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that document.body autocompletes in the web console.

function test() {
  addTab("data:text/html;charset=utf-8,Web Console autocompletion bug in document.body");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

var gHUD;

function consoleOpened(aHud) {
  gHUD = aHud;
  let jsterm = gHUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;

  let tmp = {};
  Cu.import("resource:///modules/WebConsoleUtils.jsm", tmp);
  let WCU = tmp.WebConsoleUtils;
  tmp = null;

  ok(!popup.isOpen, "popup is not open");

  popup._panel.addEventListener("popupshown", function onShown() {
    popup._panel.removeEventListener("popupshown", onShown, false);

    ok(popup.isOpen, "popup is open");

    let props = WCU.namesAndValuesOf(content.wrappedJSObject.document.body);
    is(popup.itemCount, props.length, "popup.itemCount is correct");

    popup._panel.addEventListener("popuphidden", autocompletePopupHidden, false);

    EventUtils.synthesizeKey("VK_ESCAPE", {});
  }, false);

  jsterm.setInputValue("document.body");
  EventUtils.synthesizeKey(".", {});
}

function autocompletePopupHidden()
{
  let jsterm = gHUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;
  let inputNode = jsterm.inputNode;

  popup._panel.removeEventListener("popuphidden", autocompletePopupHidden, false);

  ok(!popup.isOpen, "popup is not open");
  let inputStr = "document.b";
  jsterm.setInputValue(inputStr);
  EventUtils.synthesizeKey("o", {});
  let testStr = inputStr.replace(/./g, " ") + " ";

  waitForSuccess({
    name: "autocomplete shows document.body",
    validatorFn: function()
    {
      return completeNode.value == testStr + "dy";
    },
    successFn: testPropertyPanel,
    failureFn: finishTest,
  });
}

function testPropertyPanel()
{
  let jsterm = gHUD.jsterm;
  jsterm.clearOutput();
  jsterm.setInputValue("document");
  jsterm.execute();

  waitForSuccess({
    name: "jsterm document object output",
    validatorFn: function()
    {
      return gHUD.outputNode.querySelector(".webconsole-msg-output");
    },
    successFn: function()
    {
      document.addEventListener("popupshown", function onShown(aEvent) {
        document.removeEventListener("popupshown", onShown, false);
        executeSoon(propertyPanelShown.bind(null, aEvent.target));
      }, false);

      let node = gHUD.outputNode.querySelector(".webconsole-msg-output");
      EventUtils.synthesizeMouse(node, 2, 2, {});
    },
    failureFn: finishTest,
  });
}

function propertyPanelShown(aPanel)
{
  let tree = aPanel.querySelector("tree");
  let view = tree.view;
  let col = tree.columns[0];
  ok(view.rowCount, "Property Panel rowCount");

  let foundBody = false;
  let propPanelProps = [];
  for (let idx = 0; idx < view.rowCount; ++idx) {
    let text = view.getCellText(idx, col);
    if (text == "body: HTMLBodyElement" || text == "body: Object")
      foundBody = true;
    propPanelProps.push(text.split(":")[0]);
  }

  // NB: We pull the properties off the prototype, rather than off object itself,
  // so that expandos like |constructor|, which the propPanel can't see, are not
  // included.
  for (let prop in Object.getPrototypeOf(content.document)) {
    if (prop == "inputEncoding") {
      continue;
    }
    ok(propPanelProps.indexOf(prop) != -1, "Property |" + prop + "| should be reflected in propertyPanel");
  }

  ok(foundBody, "found document.body");

  executeSoon(finishTest);
}

