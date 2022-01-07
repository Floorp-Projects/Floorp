/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ONBOARDING_CHOICE } = ChromeUtils.import(
  "resource:///modules/UrlbarQuickSuggest.jsm"
);

// Number of height pixels to switch to compact mode to avoid showing scrollbars
// on the non-compact mode. This is based on the natural height of the full-size
// section height 587px + padding-block-start 32px + padding-block-end 32px +
// the approximate height of the tab bar 44px.
const COMPACT_MODE_HEIGHT = 587 + 32 * 2 + 44;

document.addEventListener("DOMContentLoaded", () => {
  addSubmitListener(document.getElementById("onboardingNext"), () => {
    document.getElementById("introduction-section").classList.add("inactive");
    document.getElementById("main-section").classList.add("active");
  });
  addSubmitListener(document.getElementById("onboardingLearnMore"), () => {
    window.arguments[0].choice = ONBOARDING_CHOICE.LEARN_MORE;
    window.close();
  });
  addSubmitListener(document.getElementById("onboardingNotNow"), () => {
    window.arguments[0].choice = ONBOARDING_CHOICE.NOT_NOW;
    window.close();
  });

  const onboardingSubmit = document.getElementById("onboardingSubmit");
  const onboardingAccept = document.getElementById("onboardingAccept");
  const onboardingReject = document.getElementById("onboardingReject");
  function optionChangeListener() {
    onboardingSubmit.removeAttribute("disabled");
    onboardingAccept
      .closest(".option")
      .classList.toggle("selected", onboardingAccept.checked);
    onboardingReject
      .closest(".option")
      .classList.toggle("selected", !onboardingAccept.checked);
  }
  onboardingAccept.addEventListener("change", optionChangeListener);
  onboardingReject.addEventListener("change", optionChangeListener);

  function submitListener() {
    if (!onboardingAccept.checked && !onboardingReject.checked) {
      return;
    }

    window.arguments[0].choice = onboardingAccept.checked
      ? ONBOARDING_CHOICE.ACCEPT
      : ONBOARDING_CHOICE.REJECT;
    window.close();
  }
  addSubmitListener(onboardingSubmit, submitListener);
  onboardingAccept.addEventListener("keydown", e => {
    if (e.keyCode == e.DOM_VK_RETURN) {
      submitListener();
    }
  });
  onboardingReject.addEventListener("keydown", e => {
    if (e.keyCode == e.DOM_VK_RETURN) {
      submitListener();
    }
  });

  if (window.outerHeight < COMPACT_MODE_HEIGHT) {
    document.body.classList.add("compact");
  }
});

function addSubmitListener(element, listener) {
  element.addEventListener("click", listener);
  element.addEventListener("keydown", e => {
    if (e.keyCode == e.DOM_VK_RETURN) {
      listener();
    }
  });
}
