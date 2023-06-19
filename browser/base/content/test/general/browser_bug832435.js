/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

add_task(async function test() {
  gBrowser.selectedBrowser.focus();
  await UrlbarTestUtils.inputIntoURLBar(
    window,
    "javascript: var foo = '11111111'; "
  );
  ok(gURLBar.focused, "Address bar is focused");
  EventUtils.synthesizeKey("VK_RETURN");

  // javscript: URIs are evaluated async.
  await new Promise(resolve => SimpleTest.executeSoon(resolve));
  ok(true, "Evaluated without crashing");
});
