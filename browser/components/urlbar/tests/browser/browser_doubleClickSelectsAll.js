/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function doubleClick(target) {
  let promise = BrowserTestUtils.waitForEvent(target, "dblclick");
  EventUtils.synthesizeMouseAtCenter(target, { clickCount: 1 }, target.ownerGlobal);
  EventUtils.synthesizeMouseAtCenter(target, { clickCount: 2 }, target.ownerGlobal);
  return promise;
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.urlbar.clickSelectsAll", false],
    ["browser.urlbar.doubleClickSelectsAll", true],
  ]});

  let url = "about:mozilla";
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab({ gBrowser: win.gBrowser, url });

  await doubleClick(win.gURLBar.inputField);
  is(win.gURLBar.selectionStart, 0, "Selection should start at the beginning of the urlbar value");
  is(win.gURLBar.selectionEnd, url.length, "Selection should end at the end of the urlbar value");

  win.close();
});

