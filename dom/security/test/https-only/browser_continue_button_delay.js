"use strict";

function waitForEnabledButton() {
  return new Promise(resolve => {
    const button = content.document.getElementById("openInsecure");
    const observer = new content.MutationObserver(mutations => {
      for (const mutation of mutations) {
        if (
          mutation.type === "attributes" &&
          mutation.attributeName === "inert" &&
          !mutation.target.inert
        ) {
          resolve();
        }
      }
    });
    observer.observe(button, { attributeFilter: ["inert"] });
    ok(
      button.inert,
      "The 'Continue to HTTP Site' button should be inert right after the error page is loaded."
    );
  });
}

add_task(async function () {
  waitForExplicitFinish();

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  const specifiedDelay = Services.prefs.getIntPref(
    "security.dialog_enable_delay",
    1000
  );

  let loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  info("Loading insecure page");
  const startTime = Date.now();
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    // We specifically want a insecure url here that will fail to upgrade
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://untrusted.example.com:80"
  );
  await loaded;
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], waitForEnabledButton);
  const endTime = Date.now();

  const observedDelay = endTime - startTime;

  ok(
    observedDelay > specifiedDelay - 100,
    `The observed delay (${observedDelay}ms) should be roughly the same or greater than the delay specified in "security.dialog_enable_delay" (${specifiedDelay}ms)`
  );

  finish();
});
