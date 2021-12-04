/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  AddonManager,
  document: gDoc,
  Services,
  XPCOMUtils,
} = window.docShell.chromeEventHandler.ownerGlobal;
XPCOMUtils.defineLazyModuleGetters(this, {
  BuiltInThemes: "resource:///modules/BuiltInThemes.jsm",
});

const HOMEPAGE_PREF = "browser.startup.homepage";
const NEWTAB_PREF = "browser.newtabpage.enabled";

// Strings for various elements with matching ids on each screen.
const SCREEN_STRINGS = [
  {
    title: "upgrade-dialog-colorway-title",
    subtitle: "upgrade-dialog-start-subtitle",
    primary: "upgrade-dialog-colorway-primary-button",
    secondary: "upgrade-dialog-colorway-secondary-button",
  },
  {
    title: "upgrade-dialog-thankyou-title",
    subtitle: "upgrade-dialog-thankyou-subtitle",
    primary: "upgrade-dialog-thankyou-primary-button",
  },
];

// Themes that can be selected by the button with matching index.
const THEME_IDS = [
  [
    "firefox-compact-light@mozilla.org",
    "default-theme@mozilla.org",
    "firefox-compact-dark@mozilla.org",
  ],
  [
    "abstract-soft-colorway@mozilla.org",
    "abstract-balanced-colorway@mozilla.org",
    "abstract-bold-colorway@mozilla.org",
  ],
  [
    "cheers-soft-colorway@mozilla.org",
    "cheers-balanced-colorway@mozilla.org",
    "cheers-bold-colorway@mozilla.org",
  ],
  [
    "foto-soft-colorway@mozilla.org",
    "foto-balanced-colorway@mozilla.org",
    "foto-bold-colorway@mozilla.org",
  ],
  [
    "lush-soft-colorway@mozilla.org",
    "lush-balanced-colorway@mozilla.org",
    "lush-bold-colorway@mozilla.org",
  ],
  [
    "graffiti-soft-colorway@mozilla.org",
    "graffiti-balanced-colorway@mozilla.org",
    "graffiti-bold-colorway@mozilla.org",
  ],
  [
    "elemental-soft-colorway@mozilla.org",
    "elemental-balanced-colorway@mozilla.org",
    "elemental-bold-colorway@mozilla.org",
  ],
];

// Callbacks to run when the dialog closes (both from this file or externally).
const CLEANUP = [];
addEventListener("pagehide", () => CLEANUP.forEach(f => f()), { once: true });

// Save the previous theme to revert to it.
let gPrevTheme = AddonManager.getAddonsByTypes(["theme"]).then(addons => {
  for (const { id, isActive } of addons) {
    if (isActive) {
      // Assume we need to revert the theme unless cleared.
      CLEANUP.push(() => gPrevTheme && enableTheme(id));
      return { id };
    }
  }

  // If there were no active themes, the default will be selected.
  return { id: THEME_IDS[0][1] };
});

// Helper to switch themes.
async function enableTheme(id) {
  await BuiltInThemes.ensureBuiltInThemes();
  (await AddonManager.getAddonByID(id)).enable();
}

// Helper to show the theme in chrome with an adjusted modal backdrop.
function adjustModalBackdrop() {
  const { classList } = gDoc.getElementById("window-modal-dialog");
  classList.add("showToolbar");
  CLEANUP.push(() => classList.remove("showToolbar"));
}

// Helper to record various events from the dialog content.
function recordEvent(obj, val) {
  Services.telemetry.recordEvent("upgrade_dialog", "content", obj, `${val}`);
}

// Assume the dialog closes from an external trigger unless this helper is used.
let gCloseReason = "external";
CLEANUP.push(() => recordEvent("close", gCloseReason));
function closeDialog(reason) {
  gCloseReason = reason;
  close();
}

// Detect quit requests to proactively dismiss to allow the quit prompt to show
// as otherwise gDialogBox queues the prompt as these share the same display.
const QUIT_TOPIC = "quit-application-requested";
const QUIT_OBSERVER = () => closeDialog(QUIT_TOPIC);
Services.obs.addObserver(QUIT_OBSERVER, QUIT_TOPIC);
CLEANUP.push(() => Services.obs.removeObserver(QUIT_OBSERVER, QUIT_TOPIC));

// Helper to trigger transitions with animation frames.
function triggerTransition(callback) {
  requestAnimationFrame(() => requestAnimationFrame(callback));
}

// Hook up dynamic behaviors of the dialog.
function onLoad(ready) {
  // Testing doesn't have time to overwrite this new window's random method.
  if (Cu.isInAutomation) {
    Math.random = () => 0;
  }

  const { body } = document;
  const logo = document.querySelector(".logo");
  const title = document.getElementById("title");
  const subtitle = document.getElementById("subtitle");
  const colorways = document.querySelector(".colorways");
  const themes = document.querySelector(".themes");
  const variations = document.querySelector(".variations");
  const checkbox = document.querySelector(".checkbox");
  const primary = document.getElementById("primary");
  const secondary = document.getElementById("secondary");

  // Show a new set of colorway variations based on the selected theme.
  function showVariations(themeRadio) {
    let l10nIds, themeName;
    const { l10nArgs } = themeRadio.dataset;
    if (l10nArgs) {
      // Directly set the header with unlocalized colorway name.
      const { colorwayName } = JSON.parse(l10nArgs);
      variations.firstElementChild.textContent = colorwayName;

      l10nIds = [
        "upgrade-dialog-colorway-variation-soft",
        "upgrade-dialog-colorway-variation-balanced",
        "upgrade-dialog-colorway-variation-bold",
      ];
      themeName = colorwayName.toLowerCase();
    } else {
      l10nIds = [
        "upgrade-dialog-colorway-default-theme",
        "upgrade-dialog-theme-light",
        "upgrade-dialog-colorway-theme-auto",
        "upgrade-dialog-theme-dark",
      ];
      themeName = "default";
    }

    // Show the appropriate variation options and header text too.
    l10nIds.reduceRight((node, l10nId) => {
      // Clear the previous id as textContent might have changed.
      node.dataset.l10nId = "";
      document.l10n.setAttributes(node, l10nId);
      return node.previousElementSibling;
    }, variations.lastElementChild);

    // Transition in the background image.
    variations.classList = `variations ${themeName} in`;
    triggerTransition(() => variations.classList.remove("in"));

    // Let testing know the variations are set.
    dispatchEvent(new CustomEvent("variations"));
  }

  // Watch for fluent-dom translating variation inputs to set aria-label. This
  // is because using the textContent as the label for an <input type="radio">
  // is non-standard and not exposed by a11y. The correct fix is to change the
  // l10n strings to include aria-label, but we don't want to change the l10n
  // strings in beta. We can't manually set aria-label too early because Fluent
  // will clobber it, so we watch for mutation.
  new MutationObserver(list =>
    list.forEach(({ target }) => {
      if (target.type === "radio" && !target.hasAttribute("aria-label")) {
        target.setAttribute("aria-label", target.textContent);
      }
    })
  ).observe(variations, { attributes: true, subtree: true });

  // Prepare showing the colorways screen.
  function showColorways() {
    // Use bold variant (index 2) if current theme is dark; otherwise soft (0).
    variations.querySelectorAll("input")[
      2 * matchMedia("(prefers-color-scheme: dark)").matches
    ].checked = true;

    // Enable the theme and variant based on the current selection.
    const getVariantIndex = () =>
      [...variations.children].indexOf(variations.querySelector(":checked")) -
      1;
    const enableVariant = () =>
      enableTheme(
        THEME_IDS[variations.getAttribute("next")][getVariantIndex()]
      );

    // Prepare random theme selection that's not (first) default.
    const random = Math.floor(Math.random() * (THEME_IDS.length - 1)) + 1;
    const selected = themes.children[random];
    selected.checked = true;
    recordEvent("show", `random-${random}`);

    // Transition in the starting random theme.
    triggerTransition(() => variations.setAttribute("next", random));
    setTimeout(() => {
      enableVariant();
      showVariations(selected);
    }, 400);

    // Wait for variation button clicks.
    variations.addEventListener("click", ({ target: button }) => {
      // Ignore clicks of whitespace / not-radio-button.
      if (button.type === "radio") {
        enableVariant();
        recordEvent("theme", `variant-${getVariantIndex()}`);
      }
    });

    // Wait for theme button clicks.
    let nextButton;
    themes.addEventListener("click", ({ target: button }) => {
      // Ignore clicks on whitespace of the container around theme buttons.
      if (button.parentNode === themes) {
        // Cover up content with the next color circle.
        variations.setAttribute("next", [...themes.children].indexOf(button));

        // Start a transition out while avoiding duplicate transitions.
        if (!nextButton) {
          variations.classList.add("out");
          setTimeout(() => {
            variations.classList.remove("out");

            // Enable the theme of the now-selected (next) color.
            enableVariant();
            recordEvent("theme", `theme-${variations.getAttribute("next")}`);

            // Transition in the next variations.
            showVariations(nextButton);
            nextButton = null;
          }, 500);
        }

        // Save the currently selected button to activate after transition.
        nextButton = button;
      }
    });

    // Load resource: theme swatches with permission.
    [...themes.children].forEach(input => {
      new Image().src = getComputedStyle(
        input,
        "::before"
      ).backgroundImage.match(/resource:[^"]+/)?.[0];
    });

    // Update content and backdrop for theme screen.
    colorways.classList.remove("hidden");
    adjustModalBackdrop();

    // Show checkbox to revert homepage or newtab if customized.
    if (
      Services.prefs.prefHasUserValue(HOMEPAGE_PREF) ||
      Services.prefs.prefHasUserValue(NEWTAB_PREF)
    ) {
      checkbox.classList.remove("hidden");
      recordEvent("show", checkbox.lastElementChild.dataset.l10nId);
    } else {
      checkbox.remove();
      checkbox.firstElementChild.checked = false;
    }

    return selected;
  }

  // Handle completion of colorways screen.
  function removeColorways() {
    body.classList.add("confetti");
    logo.classList.remove("hidden");
    colorways.remove();
    checkbox.remove();
  }

  // Handle checkbox being checked.
  function handleCheckbox() {
    // Revert both homepage and newtab if still checked (potentially doing
    // nothing if each pref is already the default value).
    if (checkbox.firstElementChild.checked) {
      Services.prefs.clearUserPref(HOMEPAGE_PREF);
      Services.prefs.clearUserPref(NEWTAB_PREF);
      recordEvent("button", checkbox.lastElementChild.dataset.l10nId);
    }
  }

  // Update the screen content and handle actions.
  let busy = false;
  let current = -1;
  (async function advance({ target } = {}) {
    // Record which button was clicked.
    if (target) {
      recordEvent("button", (busy ? "busy:" : "") + target.dataset.l10nId);
    }

    // Disallow multiple concurrent advances, e.g., double click while the
    // first callback is still busy awaiting.
    if (busy) {
      return;
    }
    busy = true;

    // Set the correct target for keyboard focus.
    let toFocus = primary;

    // Move to the next screen and perform screen-specific behavior / setup.
    if (++current === 0) {
      // Wait for main button clicks on each screen.
      primary.addEventListener("click", advance);
      secondary.addEventListener("click", advance);

      recordEvent("show", `${SCREEN_STRINGS.length}-screens`);
      await document.l10n.ready;

      // Make sure we have the previous theme before randomly selecting new one.
      await gPrevTheme;
      toFocus = showColorways();
    } else {
      // Handle actions and setup for not-first and not-last screens.
      const { l10nId } = target.dataset;
      switch (l10nId) {
        // New theme is confirmed, so don't revert to previous.
        case "upgrade-dialog-colorway-primary-button":
          gPrevTheme = null;
          handleCheckbox();
          removeColorways();
          break;

        // Immediately revert to existing theme.
        case "upgrade-dialog-colorway-secondary-button":
          enableTheme((await gPrevTheme).id);
          removeColorways();
          break;

        // User manually completed the last step.
        case "upgrade-dialog-thankyou-primary-button":
          closeDialog("complete");
          return;
      }
    }

    // Automatically close the last screen.
    if (current === SCREEN_STRINGS.length - 1) {
      setTimeout(() => closeDialog("autoclose"), 20000);
    }

    // Update strings for reused elements that change between screens.
    const strings = SCREEN_STRINGS[current];
    const translatedElements = [];
    for (let el of [title, subtitle, primary, secondary]) {
      const stringId = strings[el.id];
      if (stringId) {
        document.l10n.setAttributes(el, stringId);
        translatedElements.push(el);
        el.classList.remove("hidden");
        el.disabled = false;
      } else {
        el.classList.add("hidden");
        // Disabled inputs take up space to avoid shifting layout.
        el.disabled = true;
        // Avoid screen readers from seeing this too.
        el.textContent = "";
      }
    }

    // Wait for initial translations to load before getting sizing information.
    await document.l10n.translateElements(translatedElements);
    requestAnimationFrame(() => {
      // Ensure the correct button is focused on each screen.
      toFocus.focus({ preventFocusRing: true });

      // Save first screen height, so later screens can flex and anchor content.
      if (current === 0) {
        body.style.minHeight = getComputedStyle(body).height;

        // Indicate to SubDialog that we're done sizing the first screen.
        ready();
      }

      // Let testing know the screen is ready to continue.
      dispatchEvent(new CustomEvent("ready"));
    });

    // Record which screen was shown identified by the primary button.
    recordEvent("show", primary.dataset.l10nId);

    busy = false;
  })();
}

// Indicate when we're ready to show and size (async localized) content.
document.mozSubdialogReady = new Promise(resolve =>
  document.addEventListener("DOMContentLoaded", () => onLoad(resolve), {
    once: true,
  })
);
