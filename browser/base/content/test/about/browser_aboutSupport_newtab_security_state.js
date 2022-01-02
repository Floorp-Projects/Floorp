/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function checkIdentityOfAboutSupport() {
  let tab = gBrowser.loadOneTab("about:support", {
    referrerURI: null,
    inBackground: false,
    allowThirdPartyFixup: false,
    relatedToCurrent: false,
    skipAnimation: true,
    allowMixedContent: false,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });

  await promiseTabLoaded(tab);
  let identityBox = document.getElementById("identity-box");
  is(identityBox.className, "chromeUI", "Should know that we're chrome.");
  gBrowser.removeTab(tab);
});
