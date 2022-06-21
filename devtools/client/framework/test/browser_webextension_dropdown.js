/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* globals browser */

const URL =
  "data:text/html;charset=utf8,test for drop down menu in devtools extension";

add_task(async function runTest() {
  const extension = await startupExtension();

  const tab = await addTab(URL);
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "webconsole",
  });
  const { Toolbox } = require("devtools/client/framework/toolbox");
  await toolbox.switchHost(Toolbox.HostType.WINDOW);

  await extension.awaitMessage("devtools_page_loaded");

  const toolboxAdditionalTools = toolbox.getAdditionalTools();
  is(
    toolboxAdditionalTools.length,
    1,
    "Got the expected number of toolbox specific panel registered."
  );

  const panelId = toolboxAdditionalTools[0].id;

  await gDevTools.showToolboxForTab(tab, { toolId: panelId });

  await extension.awaitMessage("devtools_panel_loaded");

  const panel = findExtensionPanel();
  ok(panel, "found extension panel");

  const iframe = panel.firstChild;
  const popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(
    iframe.contentWindow
  );

  const browser = iframe.contentDocument.getElementById(
    "webext-panels-browser"
  );
  ok(browser, "found extension panel browser");

  await ContentTask.spawn(browser, null, async function() {
    const menu = content.document.getElementById("menu");
    const event = new content.MouseEvent("mousedown");
    menu.dispatchEvent(event);
  });

  await popupShownPromise;
  info("popup is shown");

  await toolbox.destroy();

  gBrowser.removeCurrentTab();

  await extension.unload();
});

async function startupExtension() {
  async function devtools_page() {
    await browser.devtools.panels.create(
      "drop",
      "/icon.png",
      "/devtools_panel.html"
    );
    browser.test.sendMessage("devtools_page_loaded");
  }

  async function devtools_panel() {
    browser.test.sendMessage("devtools_panel_loaded");
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <script src="devtools_page.js"></script>
          </body>
        </html>`,
      "devtools_page.js": devtools_page,
      "icon.png": "",
      "devtools_panel.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <select id="menu">
              <option value="A" selected>A</option>
              <option value="B">B</option>
              <option value="C">C</option>
            </select>
            <script src="devtools_panel.js"></script>
          </body>
        </html>`,
      "devtools_panel.js": devtools_panel,
    },
  });

  await extension.startup();

  return extension;
}

function findExtensionPanel() {
  const win = Services.wm.getMostRecentWindow("devtools:toolbox");
  ok(win, "toolbox separate window exists");

  const iframe = win.document.querySelector(".devtools-toolbox-window-iframe");
  const deck = iframe.contentDocument.getElementById("toolbox-deck");
  for (const box of deck.childNodes) {
    if (box.id && box.id.startsWith("toolbox-panel-webext-devtools-panel")) {
      return box;
    }
  }
  return null;
}
