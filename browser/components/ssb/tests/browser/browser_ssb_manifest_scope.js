/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that a site's manifest affects the scope of a ssb.

function build_task(page, linkId, external, preload) {
  let expectedTarget = linkId + "/final.html";

  add_task(async () => {
    let ssb;

    info(`Loading ${page} (${preload})`);
    if (preload) {
      // Loading via a browser will initialize the SSB with the correct manifest.
      await BrowserTestUtils.openNewForegroundTab({
        gBrowser,
        url: gHttpsTestRoot + page,
      });

      ssb = await openSSBFromBrowserWindow();
    } else {
      // The manifest will be loaded once the initial page loads.
      ssb = await openSSB(gHttpsTestRoot + page);
    }

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

    info(`Clicking #${linkId}`);
    await BrowserTestUtils.synthesizeMouseAtCenter(
      `#${linkId}`,
      {},
      getBrowser(ssb)
    );

    await promise;
    await BrowserTestUtils.closeWindow(ssb);
  });
}

function make_all_tasks(preload) {
  /**
   * Arguments are:
   *
   * * Page to load.
   * * Link ID to click in the page.
   * * Is that link expected to point to an external site (i.e. should be retargeted).
   * * true to create the SSB from a loaded browser, false to create it from a URL.
   *     The latter implies that the manifest won't initially be available.
   */
  build_task("site1/simple.html", "site1", false, preload);
  build_task("site1/simple.html", "site2", true, preload);
  build_task("site1/empty.html", "site1", false, preload);
  build_task("site1/empty.html", "site2", true, preload);
  build_task("site1/allhost.html", "site1", false, preload);
  build_task("site1/allhost.html", "site2", false, preload);
  build_task("site1/bad.html", "site1", false, preload);
  build_task("site1/bad.html", "site2", true, preload);
}

make_all_tasks(true);
make_all_tasks(false);
