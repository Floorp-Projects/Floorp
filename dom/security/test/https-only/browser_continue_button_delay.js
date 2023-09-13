"use strict";

function wait(ms) {
  // We have to check that the button gets enabled after a certain time
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function verifyErrorPage(expectErrorPage = true) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [expectErrorPage],
    async function (_expectErrorPage) {
      let doc = content.document;
      let innerHTML = doc.body.innerHTML;
      let errorPageL10nId = "about-httpsonly-title-alert";

      is(
        innerHTML.includes(errorPageL10nId) &&
          doc.documentURI.startsWith("about:httpsonlyerror"),
        _expectErrorPage,
        "we should be on the https-only error page"
      );
    }
  );
}

async function continueToInsecureSite() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let button = content.document.getElementById("openInsecure");
    ok(button, "'Continue to HTTP Site' button exist");
    info("Pressing 'Continue to HTTP Site' button");
    if (button) {
      button.click();
    }
  });
}

async function runTest(timeout, expectErrorPage) {
  let loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  info("Loading insecure page");
  // We specifically want a insecure url here that will fail to upgrade
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  BrowserTestUtils.loadURIString(gBrowser, "http://untrusted.example.com:80");
  await loaded;
  await verifyErrorPage(true);
  await wait(timeout);
  await continueToInsecureSite();
  await wait(500);
  await verifyErrorPage(expectErrorPage);
}

add_task(async function () {
  waitForExplicitFinish();

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  const delay = Services.prefs.getIntPref("security.dialog_enable_delay", 0);

  // 0.5s before the delay specified in `security.dialog_enable_delay`,
  // the button should still be disabled, so after pressing it we should
  // still be on the error page.
  await runTest(Math.max(100, delay - 500), true);

  // 0.5s after the delay specified in `security.dialog_enable_delay`, the
  // button should be available, so pressing it should result in us
  // leaving the error page.
  await runTest(delay + 500, false);

  finish();
});
