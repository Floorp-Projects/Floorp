/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that a site's manifest affects the scope of a ssb.

function build_task(page, linkId, external) {
  let expectedTarget = linkId + "/final.html";

  add_task(async () => {
    await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: gHttpsTestRoot + page,
    });

    let ssb = await openSSBFromBrowserWindow();

    let promise;
    if (external) {
      promise = expectTabLoad(ssb).then(tab => {
        Assert.equal(
          tab.linkedBrowser.currentURI.spec,
          gHttpsTestRoot + expectedTarget,
          "Should have loaded the right uri."
        );
        BrowserTestUtils.removeTab(tab);
      });
    } else {
      promise = expectSSBLoad(ssb).then(() => {
        Assert.equal(
          getBrowser(ssb).currentURI.spec,
          gHttpsTestRoot + expectedTarget,
          "Should have loaded the right uri."
        );
      });
    }

    await BrowserTestUtils.synthesizeMouseAtCenter(
      `#${linkId}`,
      {},
      getBrowser(ssb)
    );

    await promise;
    await BrowserTestUtils.closeWindow(ssb);
  });
}

/**
 * Arguments are:
 *
 * * Page to load.
 * * Link ID to click in the page.
 * * Is that link expected to point to an external site (i.e. should be retargeted).
 */
build_task("site1/simple.html", "site1", false);
build_task("site1/simple.html", "site2", true);
build_task("site1/empty.html", "site1", false);
build_task("site1/empty.html", "site2", true);
build_task("site1/allhost.html", "site1", false);
build_task("site1/allhost.html", "site2", false);
