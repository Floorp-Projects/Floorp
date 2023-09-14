/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests that the identity icons don't flicker when navigating,
 * i.e. the box should show no intermediate identity state. */

add_task(async function test() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:robots",
    true
  );
  let identityBox = document.getElementById("identity-box");

  is(
    identityBox.className,
    "localResource",
    "identity box has the correct class"
  );

  let observerOptions = {
    attributes: true,
    attributeFilter: ["class"],
  };
  let classChanges = 0;

  let observer = new MutationObserver(function (mutations) {
    for (let mutation of mutations) {
      is(mutation.type, "attributes");
      is(mutation.attributeName, "class");
      classChanges++;
      is(
        identityBox.className,
        "verifiedDomain",
        "identity box class changed correctly"
      );
    }
  });
  observer.observe(identityBox, observerOptions);

  let loaded = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    "https://example.com/"
  );
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "https://example.com"
  );
  await loaded;

  is(classChanges, 1, "Changed the className once");
  observer.disconnect();
  BrowserTestUtils.removeTab(tab);
});
