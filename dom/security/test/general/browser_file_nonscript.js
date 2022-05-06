/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_fileurl_nonscript_load() {
  let file = getChromeDir(getResolvedURI(gTestPath));
  file.append("file_loads_nonscript.html");
  let uriString = Services.io.newFileURI(file).spec;

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uriString);
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
  });

  let ran = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.window.wrappedJSObject.ran;
  });

  is(ran, "error", "Script should not have run");
});
