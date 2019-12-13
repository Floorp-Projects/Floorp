/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function testDirectLoad(target, checker) {
  let ssb = await openSSB(gHttpsTestRoot + "test_page.html#" + target);

  let promise = checker(ssb);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#direct",
    {},
    getBrowser(ssb)
  );

  await promise;
  await BrowserTestUtils.closeWindow(ssb);
}

// A link that should load inside the ssb
add_task(async function local() {
  await testDirectLoad(gHttpsTestRoot + "empty_page.html", async ssb => {
    try {
      await expectSSBLoad(ssb);
      Assert.equal(
        getBrowser(ssb).currentURI.spec,
        gHttpsTestRoot + "empty_page.html",
        "Should have loaded the right uri."
      );
    } catch (e) {
      // Any error will already have logged a failure.
    }
  });
});

// A link to an insecure site should load outside the ssb
add_task(async function insecure() {
  await testDirectLoad(gHttpTestRoot + "empty_page.html", async ssb => {
    try {
      let tab = await expectTabLoad(ssb);
      Assert.equal(
        tab.linkedBrowser.currentURI.spec,
        gHttpTestRoot + "empty_page.html",
        "Should have loaded the right uri."
      );
      BrowserTestUtils.removeTab(tab);
    } catch (e) {
      // Any error will already have logged a failure.
    }
  });
});

// A link to a different host should load outside the ssb
add_task(async function external() {
  await testDirectLoad(gHttpsOtherRoot + "empty_page.html", async ssb => {
    try {
      let tab = await expectTabLoad(ssb);
      Assert.equal(
        tab.linkedBrowser.currentURI.spec,
        gHttpsOtherRoot + "empty_page.html",
        "Should have loaded the right uri."
      );
      BrowserTestUtils.removeTab(tab);
    } catch (e) {
      // Any error will already have logged a failure.
    }
  });
});
