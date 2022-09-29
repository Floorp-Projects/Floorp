/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppMenuNotifications } = ChromeUtils.importESModule(
  "resource://gre/modules/AppMenuNotifications.sys.mjs"
);

add_task(async function test_unconfigured_no_badge() {
  const oldUIState = UIState.get;

  UIState.get = () => ({
    status: UIState.STATUS_NOT_CONFIGURED,
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  checkFxABadge(false);

  UIState.get = oldUIState;
});

add_task(async function test_signedin_no_badge() {
  const oldUIState = UIState.get;

  UIState.get = () => ({
    status: UIState.STATUS_SIGNED_IN,
    lastSync: new Date(),
    email: "foo@bar.com",
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  checkFxABadge(false);

  UIState.get = oldUIState;
});

add_task(async function test_unverified_badge_shown() {
  const oldUIState = UIState.get;

  UIState.get = () => ({
    status: UIState.STATUS_NOT_VERIFIED,
    email: "foo@bar.com",
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  checkFxABadge(true);

  UIState.get = oldUIState;
});

add_task(async function test_loginFailed_badge_shown() {
  const oldUIState = UIState.get;

  UIState.get = () => ({
    status: UIState.STATUS_LOGIN_FAILED,
    email: "foo@bar.com",
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  checkFxABadge(true);

  UIState.get = oldUIState;
});

function checkFxABadge(shouldBeShown) {
  let fxaButton = document.getElementById("fxa-toolbar-menu-button");
  let isShown =
    fxaButton.hasAttribute("badge-status") ||
    fxaButton
      .querySelector(".toolbarbutton-badge")
      .classList.contains("feature-callout");
  is(isShown, shouldBeShown, "Fxa badge shown matches expected value.");
}
