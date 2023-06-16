/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Toolbox#viewSourceInStyleEditor does fall back to view-source
 */

const TEST_URL = `data:text/html,<!DOCTYPE html><meta charset=utf8>Got no style`;
const CSS_URL = `${URL_ROOT_SSL}doc_theme.css`;

add_task(async function () {
  // start on webconsole since it doesn't have much activity so we're less vulnerable
  // to pending promises.
  const toolbox = await openNewTabAndToolbox(TEST_URL, "webconsole");

  const onTabOpen = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url => url == `view-source:${CSS_URL}`,
    true
  );

  info("View source of an existing file that isn't used by the page");
  const fileFound = await toolbox.viewSourceInStyleEditorByURL(CSS_URL, 0);
  ok(
    !fileFound,
    "viewSourceInStyleEditorByURL should resolve to false if source isn't found."
  );

  info("Waiting for view-source tab to open");
  const viewSourceTab = await onTabOpen;
  ok(true, "The view source tab was opened");
  await removeTab(viewSourceTab);

  info("Check that the current panel is the console");
  is(toolbox.currentToolId, "webconsole", "Console is still selected");

  await closeToolboxAndTab(toolbox);
});
