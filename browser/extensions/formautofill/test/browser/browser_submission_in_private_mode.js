"use strict";

const FORM_URL = "http://mochi.test:8888/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";

add_task(async function test_add_address() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let addresses = await getAddresses();

  is(addresses.length, 0, "No address in storage");

  await BrowserTestUtils.withNewTab(
    {gBrowser: privateWin.gBrowser, url: FORM_URL},
    async function(privateBrowser) {
      await ContentTask.spawn(privateBrowser, null, async function() {
        content.document.getElementById("organization").focus();
        content.document.getElementById("organization").value = "Mozilla";
        content.document.getElementById("street-address").value = "331 E. Evelyn Avenue";
        content.document.getElementById("tel").value = "1-650-903-0800";

        content.document.querySelector("input[type=submit]").click();
      });
    }
  );

  // Wait 1 second to make sure the profile has not been saved
  await new Promise(resolve => setTimeout(resolve, 1000));
  addresses = await getAddresses();
  is(addresses.length, 0, "No address saved in private browsing mode");

  await BrowserTestUtils.closeWindow(privateWin);
});
