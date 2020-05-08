/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NEW_TAB_TITLE = "PAGE 2";
const TAB_URL = "data:text/html,<title>PAGE</title>";
const NEW_TAB_URL = `data:text/html,<title>${NEW_TAB_TITLE}</title>`;

/**
 * This test file ensures that the URL input for DebugTargetInfo navigates the target to
 * the specified URL.
 */
add_task(async function() {
  const { document, tab, window } = await openAboutDebugging();

  info("Open a new background tab.");
  const debug_tab = await addTab(TAB_URL, { background: true });

  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const devToolsToolbox = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    "PAGE"
  );
  const { devtoolsDocument, devtoolsTab, devtoolsWindow } = devToolsToolbox;

  const urlInput = devtoolsDocument.querySelector(".devtools-textinput");
  await synthesizeUrlKeyInput(devToolsToolbox, urlInput, NEW_TAB_URL);

  info("Test that the debug target navigated to the specified URL.");
  const toolbox = getToolbox(devtoolsWindow);
  await waitUntil(
    () =>
      toolbox.target.url === NEW_TAB_URL &&
      debug_tab.linkedBrowser.currentURI.spec === NEW_TAB_URL
  );
  ok(true, "Target navigated.");
  ok(toolbox.target.title.includes(NEW_TAB_TITLE), "Target's title updated.");
  is(urlInput.value, NEW_TAB_URL, "Input url updated.");

  info("Remove the about:debugging tab.");
  await removeTab(tab);

  info("Remove the about:devtools-toolbox tab.");
  await removeTab(devtoolsTab);

  info("Remove the background tab");
  await removeTab(debug_tab);
});

/**
 * Synthesizes key input inside the DebugTargetInfo's URL component.
 *
 * @param {DevToolsToolbox} toolbox
 *        The DevToolsToolbox debugging the target.
 * @param {HTMLElement} inputEl
 *        The <input> element to submit the URL with.
 * @param {String}  url
 *        The URL to navigate to.
 */
async function synthesizeUrlKeyInput(toolbox, inputEl, url) {
  const { devtoolsDocument, devtoolsWindow } = toolbox;

  info("Wait for URL input to be focused.");
  const onInputFocused = waitUntil(
    () => devtoolsDocument.activeElement === inputEl
  );
  inputEl.focus();
  await onInputFocused;

  info("Synthesize entering URL into text field");
  const onInputChange = waitUntil(() => inputEl.value === url);
  for (const key of NEW_TAB_URL.split("")) {
    EventUtils.synthesizeKey(key, {}, devtoolsWindow);
  }
  await onInputChange;

  info("Submit URL to navigate to");
  EventUtils.synthesizeKey("KEY_Enter");
}
