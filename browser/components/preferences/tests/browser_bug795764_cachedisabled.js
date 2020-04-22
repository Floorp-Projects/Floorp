/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  // Adding one fake site so that the SiteDataManager would run.
  // Otherwise, without any site then it would just return so we would end up in not testing SiteDataManager.
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://www.foo.com"
  );
  Services.perms.addFromPrincipal(
    principal,
    "persistent-storage",
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  registerCleanupFunction(function() {
    Services.perms.removeFromPrincipal(principal, "persistent-storage");
  });

  SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.ui.enabled", true]],
  }).then(() => open_preferences(runTest));
}

async function runTest(win) {
  is(gBrowser.currentURI.spec, "about:preferences", "about:preferences loaded");

  let tab = win.document;
  let elements = tab.getElementById("mainPrefPane").children;

  // Test if privacy pane is opened correctly
  await win.gotoPref("panePrivacy");
  for (let element of elements) {
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "panePrivacy") {
      is_element_visible(
        element,
        `Privacy element of id=${element.id} should be visible`
      );
    } else {
      is_element_hidden(
        element,
        `Non-Privacy element of id=${element.id} should be hidden`
      );
    }
  }

  gBrowser.removeCurrentTab();
  win.close();
  finish();
}
