/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ONBOARDING_CHOICE } = ChromeUtils.importESModule(
  "resource:///modules/UrlbarQuickSuggest.sys.mjs"
);

const VARIATION_MAP = {
  a: {
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-1",
      "main-title": "firefox-suggest-onboarding-main-title-1",
      "main-description": "firefox-suggest-onboarding-main-description-1",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-1",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-1",
    },
  },
  b: {
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-2",
      "main-title": "firefox-suggest-onboarding-main-title-2",
      "main-description": "firefox-suggest-onboarding-main-description-2",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-1",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-1",
    },
  },
  c: {
    logoType: "firefox",
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-3",
      "main-title": "firefox-suggest-onboarding-main-title-3",
      "main-description": "firefox-suggest-onboarding-main-description-3",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-1",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-1",
    },
  },
  d: {
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-4",
      "main-title": "firefox-suggest-onboarding-main-title-4",
      "main-description": "firefox-suggest-onboarding-main-description-4",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-2",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-2",
    },
  },
  e: {
    logoType: "firefox",
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-5",
      "main-title": "firefox-suggest-onboarding-main-title-5",
      "main-description": "firefox-suggest-onboarding-main-description-5",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-2",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-2",
    },
  },
  f: {
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-2",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-6",
      "main-title": "firefox-suggest-onboarding-main-title-6",
      "main-description": "firefox-suggest-onboarding-main-description-6",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-2",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-2",
    },
  },
  g: {
    mainPrivacyFirst: true,
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-7",
      "main-title": "firefox-suggest-onboarding-main-title-7",
      "main-description": "firefox-suggest-onboarding-main-description-7",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-2",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-2",
    },
  },
  h: {
    logoType: "firefox",
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-2",
      "main-title": "firefox-suggest-onboarding-main-title-8",
      "main-description": "firefox-suggest-onboarding-main-description-8",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-1",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-1",
    },
  },
  "100-a": {
    introductionLayout: "layout-100",
    mainPrivacyFirst: true,
    logoType: "firefox",
    l10nUpdates: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-3",
      "introduction-title": "firefox-suggest-onboarding-main-title-9",
      "main-title": "firefox-suggest-onboarding-main-title-9",
      "main-description": "firefox-suggest-onboarding-main-description-9",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label-2",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-3",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label-2",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-3",
    },
  },
  "100-b": {
    mainPrivacyFirst: true,
    logoType: "firefox",
    l10nUpdates: {
      "main-title": "firefox-suggest-onboarding-main-title-9",
      "main-description": "firefox-suggest-onboarding-main-description-9",
      "main-accept-option-label":
        "firefox-suggest-onboarding-main-accept-option-label-2",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-3",
      "main-reject-option-label":
        "firefox-suggest-onboarding-main-reject-option-label-2",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-3",
    },
    skipIntroduction: true,
  },
};

// If the window height is smaller than this value when the dialog opens, then
// the dialog will open in compact mode. The dialog will not change modes while
// it's open even if the window height changes.
const COMPACT_MODE_HEIGHT =
  650 + // section min-height (non-compact mode)
  2 * 32 + // 2 * --section-vertical-padding (non-compact mode)
  44; // approximate height of the browser window's tab bar

// Used for test only. If links or buttons may be clicked or typed Key_Enter
// while translating l10n, cannot capture the events since not register listeners
// yet. To avoid the issue, add this flag to know the listeners are ready.
let resolveOnboardingReady;
window._quicksuggestOnboardingReady = new Promise(r => {
  resolveOnboardingReady = r;
});

document.addEventListener("DOMContentLoaded", async () => {
  await document.l10n.ready;

  const variation =
    VARIATION_MAP[window.arguments[0].variationType] || VARIATION_MAP.a;

  document.l10n.pauseObserving();
  try {
    await applyVariation(variation);
  } finally {
    document.l10n.resumeObserving();
  }

  addSubmitListener(document.getElementById("onboardingClose"), () => {
    window.arguments[0].choice = ONBOARDING_CHOICE.CLOSE_1;
    window.close();
  });
  addSubmitListener(document.getElementById("onboardingNext"), () => {
    gotoMain(variation);
  });
  addSubmitListener(document.getElementById("onboardingLearnMore"), () => {
    window.arguments[0].choice = ONBOARDING_CHOICE.LEARN_MORE_2;
    window.close();
  });
  addSubmitListener(
    document.getElementById("onboardingLearnMoreOnIntroduction"),
    () => {
      window.arguments[0].choice = ONBOARDING_CHOICE.LEARN_MORE_1;
      window.close();
    }
  );
  addSubmitListener(document.getElementById("onboardingSkipLink"), () => {
    window.arguments[0].choice = ONBOARDING_CHOICE.NOT_NOW_2;
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
      ? ONBOARDING_CHOICE.ACCEPT_2
      : ONBOARDING_CHOICE.REJECT_2;
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

  resolveOnboardingReady();
});

function gotoMain(variation) {
  window.arguments[0].visitedMain = true;

  document.getElementById("introduction-section").classList.add("inactive");
  document.getElementById("main-section").classList.add("active");

  document.body.setAttribute("aria-labelledby", "main-title");
  let ariaDescribedBy = "main-description";
  if (variation.mainPrivacyFirst) {
    ariaDescribedBy += " main-privacy-first";
  }
  document.body.setAttribute("aria-describedby", ariaDescribedBy);
}

async function applyVariation(variation) {
  if (variation.logoType) {
    for (const logo of document.querySelectorAll(".logo")) {
      logo.classList.add(variation.logoType);
    }
  }

  if (variation.mainPrivacyFirst) {
    const label = document.querySelector("#main-section .privacy-first");
    label.classList.add("active");
  }

  if (variation.l10nUpdates) {
    const translatedElements = [];
    for (const [id, newL10N] of Object.entries(variation.l10nUpdates)) {
      const element = document.getElementById(id);
      document.l10n.setAttributes(element, newL10N);
      translatedElements.push(element);
    }
    await document.l10n.translateElements(translatedElements);
  }

  if (variation.skipIntroduction) {
    document.body.classList.add("skip-introduction");
    gotoMain(variation);
  }

  if (variation.introductionLayout) {
    document
      .getElementById("introduction-section")
      .classList.add(variation.introductionLayout);
  }
}

function addSubmitListener(element, listener) {
  if (!element) {
    console.warn("Element is null on addSubmitListener");
    return;
  }
  element.addEventListener("click", listener);
  element.addEventListener("keydown", e => {
    if (e.keyCode == e.DOM_VK_RETURN) {
      listener();
    }
  });
}
