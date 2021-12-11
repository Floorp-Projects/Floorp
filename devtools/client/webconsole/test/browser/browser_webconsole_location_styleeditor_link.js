/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-location-styleeditor-link.html";

add_task(async function() {
  await pushPref("devtools.webconsole.filter.css", true);
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = await gDevTools.getToolboxForTab(gBrowser.selectedTab);

  await testViewSource(hud, toolbox, "\u2018font-weight\u2019");

  info("Selecting the console again");
  await toolbox.selectTool("webconsole");
  await testViewSource(hud, toolbox, "\u2018color\u2019");

  info("Selecting the console again");
  await toolbox.selectTool("webconsole");
  await testViewSource(hud, toolbox, "\u2018display\u2019");
});

async function testViewSource(hud, toolbox, text) {
  info(`Testing message with text "${text}"`);
  const messageNode = await waitFor(
    () => findMessage(hud, text),
    `couldn't find message containing "${text}"`
  );
  const frameLinkNode = messageNode.querySelector(
    ".message-location .frame-link"
  );
  ok(frameLinkNode, "The message does have a location link");

  const onStyleEditorSelected = toolbox.once("styleeditor-selected");

  EventUtils.sendMouseEvent(
    { type: "click" },
    messageNode.querySelector(".frame-link-filename")
  );

  const panel = await onStyleEditorSelected;
  ok(
    true,
    "The style editor is selected when clicking on the location element"
  );

  await onStyleEditorReady(panel);

  info("style editor window focused");
  const href = frameLinkNode.getAttribute("data-url");
  const line = frameLinkNode.getAttribute("data-line");
  const column = frameLinkNode.getAttribute("data-column");
  ok(line, "found source line");

  const editor = getEditorForHref(panel.UI, href);
  ok(editor, "found style editor for " + href);
  await checkCursorPosition(panel.UI, editor, line - 1, column - 1);
}

async function onStyleEditorReady(panel) {
  const win = panel.panelWindow;
  ok(win, "Style Editor Window is defined");
  ok(panel.UI, "Style Editor UI is defined");

  info("Waiting the style editor to be focused");
  return new Promise(resolve => {
    waitForFocus(function() {
      resolve();
    }, win);
  });
}

function getEditorForHref(styleEditorUI, href) {
  let foundEditor = null;
  for (const editor of styleEditorUI.editors) {
    if (editor.styleSheet.href == href) {
      foundEditor = editor;
      break;
    }
  }
  return foundEditor;
}

async function checkCursorPosition(styleEditorUI, editor, line, column) {
  info("wait for source editor to load");
  // Get out of the styleeditor-selected event loop.
  await waitForTick();

  // Get the updated line and column position if the CSS source was prettified.
  const position = editor.translateCursorPosition(line, column);
  line = position.line;
  column = position.column;

  is(editor.sourceEditor.getCursor().line, line, "correct line is selected");
  is(editor.sourceEditor.getCursor().ch, column, "correct column is selected");
  is(
    styleEditorUI.selectedStyleSheetIndex,
    editor.styleSheet.styleSheetIndex,
    "correct stylesheet is selected in the editor"
  );
}
