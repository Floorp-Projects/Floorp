/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test() {
  let ext = await loadExtension(async () => {
    let status = await browser.experiments.urlbar.getBrowserUpdateStatus();
    browser.test.sendMessage("done", status);
  });
  let status = await ext.awaitMessage("done");
  Assert.equal(status, "neverChecked");
  await ext.unload();
});
