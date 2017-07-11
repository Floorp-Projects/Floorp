/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: 'states.js', dir: MOCHITESTS_DIR },
            { name: 'role.js', dir: MOCHITESTS_DIR });

async function runTests(browser, accDoc) {
  let onFocus = waitForEvent(EVENT_FOCUS, "button");
  await ContentTask.spawn(browser, {}, () => {
    content.document.getElementById("button").focus();
  });
  let button = (await onFocus).accessible;
  testStates(button, STATE_FOCUSED);

  // Bug 1377942 - The target of the focus event changes under different
  // circumstances.
  // In e10s the focus event is the new window, in non-e10s it's the doc.
  onFocus = waitForEvent(EVENT_FOCUS, () => true);
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  // button should be blurred
  await onFocus;
  testStates(button, 0, 0, STATE_FOCUSED);

  onFocus = waitForEvent(EVENT_FOCUS, "button");
  await BrowserTestUtils.closeWindow(newWin);
  testStates((await onFocus).accessible, STATE_FOCUSED);

  onFocus = waitForEvent(EVENT_FOCUS, "body2");
  await ContentTask.spawn(browser, {}, () => {
    content.document.getElementById("editabledoc").contentWindow.document.body.focus();
  });
  testStates((await onFocus).accessible, STATE_FOCUSED);

  onFocus = waitForEvent(EVENT_FOCUS, "body2");
  newWin = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.closeWindow(newWin);
  testStates((await onFocus).accessible, STATE_FOCUSED);

  let onShow = waitForEvent(EVENT_SHOW, "alertdialog");
  onFocus = waitForEvent(EVENT_FOCUS, "alertdialog");
  await ContentTask.spawn(browser, {}, () => {
    let alertDialog = content.document.getElementById("alertdialog");
    alertDialog.style.display = "block";
    alertDialog.focus();
  });
  await onShow;
  testStates((await onFocus).accessible, STATE_FOCUSED);
}

/**
 * Accessible dialog focus testing
 */
addAccessibleTask(`
  <button id="button">button</button>
  <iframe id="editabledoc"
          src="${snippetToURL("", { id: "body2", contentEditable: "true"})}">
  </iframe>
  <div id="alertdialog" style="display: none" tabindex="-1" role="alertdialog" aria-labelledby="title2" aria-describedby="desc2">
    <div id="title2">Blah blah</div>
    <div id="desc2">Woof woof woof.</div>
    <button>Close</button>
  </div>`, runTests);
