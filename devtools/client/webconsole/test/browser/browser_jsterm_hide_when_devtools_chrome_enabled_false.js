/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Hide Browser Console JS input field if devtools.chrome.enabled is false.
 *
 * when devtools.chrome.enabled then:
 *   - browser console jsterm should be enabled
 *   - browser console object inspector properties should be set.
 *   - webconsole jsterm should be enabled
 *   - webconsole object inspector properties should be set.
 *
 * when devtools.chrome.enabled === false then
 *   - browser console jsterm should be disabled
 *   - browser console object inspector properties should be set (we used to not
 *     set them but there is no reason not to do so as the input is disabled).
 *   - webconsole jsterm should be enabled
 *   - webconsole object inspector properties should be set.
 */

"use strict";

// Needed for slow platforms (See https://bugzilla.mozilla.org/show_bug.cgi?id=1506970)
requestLongerTimeout(2);

add_task(async function() {
  let browserConsole, webConsole, objInspector;

  // Needed for the execute() function below
  await pushPref("security.allow_parent_unrestricted_js_loads", true);

  // We don't use `pushPref()` because we need to revert the same pref later
  // in the test.
  Services.prefs.setBoolPref("devtools.chrome.enabled", true);

  browserConsole = await BrowserConsoleManager.toggleBrowserConsole();
  objInspector = await logObject(browserConsole);
  testJSTermIsVisible(browserConsole);
  await testObjectInspectorPropertiesAreSet(objInspector);

  const browserTab = await addTab("data:text/html;charset=utf8,hello world");
  webConsole = await openConsole(browserTab);
  objInspector = await logObject(webConsole);
  testJSTermIsVisible(webConsole);
  await testObjectInspectorPropertiesAreSet(objInspector);
  await closeConsole(browserTab);

  await BrowserConsoleManager.toggleBrowserConsole();
  Services.prefs.setBoolPref("devtools.chrome.enabled", false);

  browserConsole = await BrowserConsoleManager.toggleBrowserConsole();
  objInspector = await logObject(browserConsole);
  testJSTermIsNotVisible(browserConsole);

  webConsole = await openConsole(browserTab);
  objInspector = await logObject(webConsole);
  testJSTermIsVisible(webConsole);
  await testObjectInspectorPropertiesAreSet(objInspector);

  info("Close webconsole and browser console");
  await closeConsole(browserTab);
  await BrowserConsoleManager.toggleBrowserConsole();
});

async function logObject(hud) {
  const prop = "browser_console_hide_jsterm_test";
  const { node } = await executeAndWaitForMessage(
    hud,
    `new Object({ ${prop}: true })`,
    prop,
    ".result"
  );
  return node.querySelector(".tree");
}

function testJSTermIsVisible(hud) {
  const inputContainer = hud.ui.window.document.querySelector(
    ".jsterm-input-container"
  );
  isnot(inputContainer.style.display, "none", "input is visible");
}

async function testObjectInspectorPropertiesAreSet(objInspector) {
  const onMutation = waitForNodeMutation(objInspector, {
    childList: true,
  });

  const arrow = objInspector.querySelector(".arrow");
  arrow.click();
  await onMutation;

  ok(
    arrow.classList.contains("expanded"),
    "The arrow of the root node of the tree is expanded after clicking on it"
  );

  const nameNode = objInspector.querySelector(
    ".node:not(.lessen) .object-label"
  );
  const container = nameNode.parentNode;
  const name = nameNode.textContent;
  const value = container.querySelector(".objectBox").textContent;

  is(name, "browser_console_hide_jsterm_test", "name is set correctly");
  is(value, "true", "value is set correctly");
}

function testJSTermIsNotVisible(hud) {
  const inputContainer = hud.ui.window.document.querySelector(
    ".jsterm-input-container"
  );
  is(inputContainer, null, "input is not in dom");
}
