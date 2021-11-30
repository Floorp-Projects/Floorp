/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ONBOARDING_CHOICE } = ChromeUtils.import(
  "resource:///modules/UrlbarQuickSuggest.jsm"
);

document.addEventListener("dialogaccept", event => {
  // dialogaccept is fired when the user presses the enter key even when an
  // element other than the accept button is focused. If another element is
  // focused, then peform its action.
  switch (document.activeElement?.id) {
    case "onboardingSettingsButton":
      window.arguments[0].choice = ONBOARDING_CHOICE.SETTINGS;
      event.preventDefault();
      window.close();
      return;
    case "onboardingNotNow":
      window.arguments[0].choice = ONBOARDING_CHOICE.NOT_NOW;
      event.preventDefault();
      window.close();
      return;
    case "onboardingLearnMore":
      window.arguments[0].choice = ONBOARDING_CHOICE.LEARN_MORE;
      event.preventDefault();
      window.close();
      return;
  }

  window.arguments[0].choice = ONBOARDING_CHOICE.ACCEPT;
});

document.addEventListener("dialogextra1", () => {
  window.arguments[0].choice = ONBOARDING_CHOICE.SETTINGS;
  window.close();
});

document.getElementById("onboardingNotNow").addEventListener("click", () => {
  window.arguments[0].choice = ONBOARDING_CHOICE.NOT_NOW;
  window.close();
});

document.getElementById("onboardingLearnMore").addEventListener("click", () => {
  window.arguments[0].choice = ONBOARDING_CHOICE.LEARN_MORE;
  window.close();
});

// If we change the system font size while displaying the dialog, problem that
// components are hidden might happen dependent on the size. To avoid it, we
// resize dialog explicitly whenever the size of internal components are changed.
window.addEventListener("load", () => {
  const resizeObserver = new ResizeObserver(() => {
    // resizeDialog() does not make window height smaller even if the total
    // height of internal components is smaller. So, we minimize the window
    // size once, then prompt to recalculate to be appropriate size.
    window.resizeBy(0, -window.innerHeight);
    window.opener.gDialogBox.dialog.resizeDialog();
  });
  resizeObserver.observe(document.getElementById("infoContainer"));
});
