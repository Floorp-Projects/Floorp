/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/*
 * Bug 922161 - Hide Browser Console JS input field if devtools.chrome.enabled
 * is false.
 * when devtools.chrome.enabled then
 *   -browser console jsterm should be enabled
 *   -browser console object inspector properties should be set.
 *   -webconsole jsterm should be enabled
 *   -webconsole object inspector properties should be set.
 *
 * when devtools.chrome.enabled == false then
 *   -browser console jsterm should be disabled
 *   -browser console object inspector properties should not be set.
 *   -webconsole jsterm should be enabled
 *   -webconsole object inspector properties should be set.
 */

"use strict";

function testObjectInspectorPropertiesAreNotSet(variablesView) {
  is(variablesView.eval, null, "vview.eval is null");
  is(variablesView.switch, null, "vview.switch is null");
  is(variablesView.delete, null, "vview.delete is null");
}

function* getVariablesView(hud) {
  function openVariablesView(event, vview) {
    deferred.resolve(vview._variablesView);
  }

  let deferred = promise.defer();
  hud.jsterm.clearOutput();
  hud.jsterm.execute("new Object({ browser_console_hide_jsterm_test: true })");

  let [message] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "Object { browser_console_hide_jsterm_test: true }",
      category: CATEGORY_OUTPUT,
    }],
  });

  hud.jsterm.once("variablesview-fetched", openVariablesView);

  let anchor = [...message.matched][0].querySelector("a");

  executeSoon(() =>
    EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow)
  );

  return deferred.promise;
}

function testJSTermIsVisible(hud) {
  let inputContainer = hud.ui.window.document
                                    .querySelector(".jsterm-input-container");
  isnot(inputContainer.style.display, "none", "input is visible");
}

function testObjectInspectorPropertiesAreSet(variablesView) {
  isnot(variablesView.eval, null, "vview.eval is set");
  isnot(variablesView.switch, null, "vview.switch is set");
  isnot(variablesView.delete, null, "vview.delete is set");
}

function testJSTermIsNotVisible(hud) {
  let inputContainer = hud.ui.window.document
                                    .querySelector(".jsterm-input-container");
  is(inputContainer.style.display, "none", "input is not visible");
}

function* testRunner() {
  let browserConsole, webConsole, variablesView;

  Services.prefs.setBoolPref("devtools.chrome.enabled", true);

  browserConsole = yield HUDService.toggleBrowserConsole();
  variablesView = yield getVariablesView(browserConsole);
  testJSTermIsVisible(browserConsole);
  testObjectInspectorPropertiesAreSet(variablesView);

  let {tab: browserTab} = yield loadTab("data:text/html;charset=utf8,hello world");
  webConsole = yield openConsole(browserTab);
  variablesView = yield getVariablesView(webConsole);
  testJSTermIsVisible(webConsole);
  testObjectInspectorPropertiesAreSet(variablesView);
  yield closeConsole(browserTab);

  yield HUDService.toggleBrowserConsole();
  Services.prefs.setBoolPref("devtools.chrome.enabled", false);

  browserConsole = yield HUDService.toggleBrowserConsole();
  variablesView = yield getVariablesView(browserConsole);
  testJSTermIsNotVisible(browserConsole);
  testObjectInspectorPropertiesAreNotSet(variablesView);

  webConsole = yield openConsole(browserTab);
  variablesView = yield getVariablesView(webConsole);
  testJSTermIsVisible(webConsole);
  testObjectInspectorPropertiesAreSet(variablesView);
  yield closeConsole(browserTab);
}

function test() {
  Task.spawn(testRunner).then(finishTest);
}
