/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test() {
  gURLBar.focus();
  await UrlbarTestUtils.inputIntoURLBar(window, "https://example.com/", {
    replaceContent: true,
  });
  gURLBar.selectionStart = 4;
  gURLBar.selectionEnd = 5;
  goDoCommand("cmd_cut");
  is(
    gURLBar.value,
    "http://example.com/",
    "location bar value after cutting 's' from https"
  );
  gURLBar.handleRevert();
});
