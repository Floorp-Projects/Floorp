/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const gTests = [test_openUILink_checkPrincipal];

function test() {
  waitForExplicitFinish();
  executeSoon(runNextTest);
}

function runNextTest() {
  if (gTests.length) {
    let testFun = gTests.shift();
    info("Running " + testFun.name);
    testFun();
  } else {
    finish();
  }
}

function test_openUILink_checkPrincipal() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/"
  )); // remote tab
  BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(async function () {
    is(
      tab.linkedBrowser.currentURI.spec,
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      "http://example.com/",
      "example.com loaded"
    );

    await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
      let channel = content.docShell.currentDocumentChannel;

      const loadingPrincipal = channel.loadInfo.loadingPrincipal;
      is(loadingPrincipal, null, "sanity: correct loadingPrincipal");
      const triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
      ok(
        triggeringPrincipal.isSystemPrincipal,
        "sanity: correct triggeringPrincipal"
      );
      const principalToInherit = channel.loadInfo.principalToInherit;
      ok(
        principalToInherit.isNullPrincipal,
        "sanity: correct principalToInherit"
      );
      ok(
        content.document.nodePrincipal.isContentPrincipal,
        "sanity: correct doc.nodePrincipal"
      );
      is(
        content.document.nodePrincipal.asciiSpec,
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        "http://example.com/",
        "sanity: correct doc.nodePrincipal URL"
      );
    });

    gBrowser.removeCurrentTab();
    runNextTest();
  });

  // Ensure we get the correct default of "allowInheritPrincipal: false" from openUILink
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  openUILink("http://example.com", null, {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal({}),
  }); // defaults to "current"
}
