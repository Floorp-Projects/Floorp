/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_fileurl_nonscript_load() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.block_fileuri_script_with_wrong_mime", true]],
  });

  let file = getChromeDir(getResolvedURI(gTestPath));
  file.append("file_loads_nonscript.html");
  let uriString = Services.io.newFileURI(file).spec;

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uriString);
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
  });

  let counter = await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    Cu.exportFunction(Assert.equal.bind(Assert), content.window, {
      defineAs: "equal",
    });
    content.window.postMessage("run", "*");

    await new Promise(resolve => {
      content.window.addEventListener("message", event => {
        if (event.data === "done") {
          resolve();
        }
      });
    });

    return content.window.wrappedJSObject.counter;
  });

  is(counter, 1, "Only one script should have run");
});
