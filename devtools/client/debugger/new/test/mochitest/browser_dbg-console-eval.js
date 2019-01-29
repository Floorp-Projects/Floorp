/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that clicking the DOM node button in any ObjectInspect
// opens the Inspector panel

function waitForConsolePanelChange(dbg) {
  const { toolbox } = dbg;

  return new Promise(resolve => {
    toolbox.getPanelWhenReady("webconsole").then(() => {
      ok(toolbox.webconsolePanel, "Console is shown.");
      resolve(toolbox.webconsolePanel);
    });
  });
}

function findMessages(win, query) {
  return Array.prototype.filter.call(
    win.document.querySelectorAll(".message"),
    e => e.innerText.includes(query)
  );
}

async function hasMessage(dbg, msg) {
  const webConsole = await dbg.toolbox.getPanel("webconsole");
  return waitFor(
    async () => findMessages(webConsole._frameWindow, msg).length > 0
  );
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");

  await selectSource(dbg, "simple2", 1);

  clickElement(dbg, "CodeMirrorLines");
  await waitForElementWithSelector(dbg, ".CodeMirror-code");

  getCM(dbg).setSelection({ line: 0, ch: 0 }, { line: 8, ch: 0 });

  rightClickElement(dbg, "CodeMirrorLines");
  selectContextMenuItem(dbg, "#node-menu-evaluate-in-console");

  await waitForConsolePanelChange(dbg);
  await hasMessage(dbg, "undefined");
});
