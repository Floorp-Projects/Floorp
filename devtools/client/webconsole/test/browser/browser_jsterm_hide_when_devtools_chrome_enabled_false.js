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

  // Setting editor mode for both webconsole and browser console as there are more
  // elements to check.
  await pushPref("devtools.webconsole.input.editor", true);
  await pushPref("devtools.browserconsole.input.editor", true);

  // Needed for the execute() function below
  await pushPref("security.allow_parent_unrestricted_js_loads", true);

  // We don't use `pushPref()` because we need to revert the same pref later
  // in the test.
  Services.prefs.setBoolPref("devtools.chrome.enabled", true);

  browserConsole = await BrowserConsoleManager.toggleBrowserConsole();
  objInspector = await logObject(browserConsole);
  testInputRelatedElementsAreVisibile(browserConsole);
  await testObjectInspectorPropertiesAreSet(objInspector);

  const browserTab = await addTab("data:text/html;charset=utf8,hello world");
  webConsole = await openConsole(browserTab);
  objInspector = await logObject(webConsole);
  testInputRelatedElementsAreVisibile(webConsole);
  await testObjectInspectorPropertiesAreSet(objInspector);
  await closeConsole(browserTab);

  await BrowserConsoleManager.toggleBrowserConsole();
  Services.prefs.setBoolPref("devtools.chrome.enabled", false);

  browserConsole = await BrowserConsoleManager.toggleBrowserConsole();
  objInspector = await logObject(browserConsole);
  testInputRelatedElementsAreNotVisibile(browserConsole);

  webConsole = await openConsole(browserTab);
  objInspector = await logObject(webConsole);
  testInputRelatedElementsAreVisibile(webConsole);
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

function getInputRelatedElements(hud) {
  const { document } = hud.ui.window;

  return {
    inputEl: document.querySelector(".jsterm-input-container"),
    eagerEvaluationEl: document.querySelector(".eager-evaluation-result"),
    editorResizerEl: document.querySelector(".editor-resizer"),
    editorToolbarEl: document.querySelector(".webconsole-editor-toolbar"),
    webConsoleAppEl: document.querySelector(".webconsole-app"),
  };
}

function testInputRelatedElementsAreVisibile(hud) {
  const {
    inputEl,
    eagerEvaluationEl,
    editorResizerEl,
    editorToolbarEl,
    webConsoleAppEl,
  } = getInputRelatedElements(hud);

  isnot(inputEl.style.display, "none", "input is visible");
  ok(eagerEvaluationEl, "eager evaluation result is in dom");
  ok(editorResizerEl, "editor resizer is in dom");
  ok(editorToolbarEl, "editor toolbar is in dom");
  ok(
    webConsoleAppEl.classList.contains("jsterm-editor") &&
      webConsoleAppEl.classList.contains("eager-evaluation"),
    "webconsole element has expected classes"
  );
}

function testInputRelatedElementsAreNotVisibile(hud) {
  const {
    inputEl,
    eagerEvaluationEl,
    editorResizerEl,
    editorToolbarEl,
    webConsoleAppEl,
  } = getInputRelatedElements(hud);

  is(inputEl, null, "input is not in dom");
  is(eagerEvaluationEl, null, "eager evaluation result is not in dom");
  is(editorResizerEl, null, "editor resizer is not in dom");
  is(editorToolbarEl, null, "editor toolbar is not in dom");
  is(
    webConsoleAppEl.classList.contains("jsterm-editor") &&
      webConsoleAppEl.classList.contains("eager-evaluation"),
    false,
    "webconsole element does not have eager evaluation nor editor classes"
  );
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
