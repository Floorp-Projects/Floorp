/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  AddonManager,
  AppConstants,
  document: gDoc,
  getShellService,
  Services,
} = window.docShell.chromeEventHandler.ownerGlobal;

// Number of height pixels to switch to compact mode to avoid showing scrollbars
// on the non-compact mode. This is based on the natural height of the full-size
// "accented" content while also accounting for the dialog container margins.
const COMPACT_MODE_HEIGHT = 679;

const SHELL = getShellService();
const IS_DEFAULT = SHELL.isDefaultBrowser();
const NEED_PIN = SHELL.doesAppNeedPin();

// Strings for various elements with matching ids on each screen.
const PIN_OR_THEME_STRING = NEED_PIN.then(pin =>
  pin
    ? "upgrade-dialog-new-primary-pin-button"
    : "upgrade-dialog-new-primary-theme-button"
);
const PRIMARY_OR_DEFAULT_STRING = NEED_PIN.then(pin =>
  pin
    ? "upgrade-dialog-new-primary-primary-button"
    : "upgrade-dialog-new-primary-default-button"
);
const SCREEN_STRINGS = [
  {
    title: "upgrade-dialog-new-title",
    primary: IS_DEFAULT ? PIN_OR_THEME_STRING : PRIMARY_OR_DEFAULT_STRING,
    secondary: "upgrade-dialog-new-secondary-button",
  },
  {
    title: "upgrade-dialog-theme-title",
    primary: "upgrade-dialog-theme-primary-button",
    secondary: "upgrade-dialog-theme-secondary-button",
  },
];

// Themes that can be selected by the button with matching index.
const THEME_IDS = [
  "default-theme@mozilla.org",
  "firefox-compact-light@mozilla.org",
  "firefox-compact-dark@mozilla.org",
  "firefox-alpenglow@mozilla.org",
];

// Callbacks to run when the dialog closes (both from this file or externally).
const CLEANUP = [];
addEventListener("pagehide", () => CLEANUP.forEach(f => f()), { once: true });

// Save the previous theme to revert to it.
let gPrevTheme = AddonManager.getAddonsByTypes(["theme"]).then(addons => {
  for (const { id, isActive, screenshots } of addons) {
    if (isActive) {
      // Only show the "keep" theme option for "other" themes.
      if (!THEME_IDS.includes(id)) {
        THEME_IDS.push(id);
      }

      // Assume we need to revert the theme unless cleared.
      CLEANUP.push(() => gPrevTheme && enableTheme(id));
      return {
        id,
        swatch: screenshots?.[0].url,
      };
    }
  }

  // If there were no active themes, the default will be selected.
  return { id: THEME_IDS[0] };
});

// Helper to switch themes.
async function enableTheme(id) {
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

// Hook up dynamic behaviors of the dialog.
function onLoad(ready) {
  // Change content for Windows 7 because non-light themes aren't quite right.
  const win7Content = AppConstants.isPlatformAndVersionAtMost("win", "6.1");

  const { body, head } = document;
  const title = document.getElementById("title");
  const subtitle = document.getElementById("subtitle");
  const items = document.querySelector(".items");
  const themes = document.querySelector(".themes");
  const primary = document.getElementById("primary");
  const secondary = document.getElementById("secondary");
  const steps = document.querySelector(".steps");

  // Update the screen content and handle actions.
  let current = -1;
  (async function advance({ target } = {}) {
    // Record which button was clicked.
    if (target) {
      recordEvent("button", target.dataset.l10nId);
    }

    // Move to the next screen and perform screen-specific behavior.
    switch (++current) {
      case 0:
        // Wait for main button clicks on each screen.
        primary.addEventListener("click", advance);
        secondary.addEventListener("click", advance);

        // Check parent window's height to determine if we should be compact.
        if (gDoc.ownerGlobal.outerHeight < COMPACT_MODE_HEIGHT) {
          body.classList.add("compact");
          recordEvent("show", "compact");
        }

        // Windows 7 has a single screen so hide steps.
        if (win7Content) {
          steps.style.visibility = "hidden";

          // If already default, reuse "Okay" for primary and hide secondary.
          if (IS_DEFAULT) {
            head.appendChild(
              head.querySelector("[rel=localization]").cloneNode()
            ).href = "browser/newtab/asrouter.ftl";
            SCREEN_STRINGS[current].primary =
              "cfr-doorhanger-doh-primary-button-2";
            secondary.style.display = "none";
          }
        }
        break;

      case 1:
        // Handle first screen conditional actions.
        if (target === primary) {
          if (!IS_DEFAULT) {
            SHELL.setAsDefault();
          }
          SHELL.pinToTaskbar();
        } else if (target === secondary && IS_DEFAULT && !(await NEED_PIN)) {
          closeDialog("early");
          return;
        }

        // First screen is the only screen for Windows 7.
        if (win7Content) {
          closeDialog("win7");
          return;
        }

        // Prepare the initial theme selection and wait for theme button clicks.
        const { id, swatch } = await gPrevTheme;
        themes.children[THEME_IDS.indexOf(id)].checked = true;
        themes.addEventListener("click", ({ target: button }) => {
          // Ignore clicks on whitespace of the container around theme buttons.
          if (button.parentNode === themes) {
            // Enable the theme of the corresponding button position.
            const index = [...themes.children].indexOf(button);
            enableTheme(THEME_IDS[index]);
            recordEvent("theme", index);
          }
        });

        // Remove the last "keep" theme option if the user didn't customize.
        if (themes.childElementCount > THEME_IDS.length) {
          themes.removeChild(themes.lastElementChild);
        } else if (swatch) {
          themes.lastElementChild.style.setProperty(
            "--theme-swatch",
            `url("${swatch}")`
          );
        }

        // Load resource: theme swatches with permission.
        [...themes.children].forEach(input => {
          new Image().src = getComputedStyle(
            input,
            "::before"
          ).backgroundImage.match(/resource:[^"]+/)?.[0];
        });

        // Update content and backdrop for theme screen.
        subtitle.remove();
        items.remove();
        themes.classList.remove("hidden");
        steps.appendChild(steps.firstChild);
        adjustModalBackdrop();
        break;

      case 2:
        // New theme is confirmed, so don't revert to previous.
        if (target === primary) {
          gPrevTheme = null;
        }

        closeDialog("complete");
        return;
    }

    // Update strings for reused elements that change between screens.
    const strings = SCREEN_STRINGS[current];
    document.l10n.setAttributes(title, strings.title);
    document.l10n.setAttributes(primary, await strings.primary);
    document.l10n.setAttributes(secondary, strings.secondary);

    // Wait for initial translations to load before getting sizing information.
    await document.l10n.ready;
    requestAnimationFrame(() => {
      // Ensure the primary button is focused on each screen.
      primary.focus();

      // Save first screen height, so later screens can flex and anchor content.
      if (current === 0) {
        body.style.minHeight = getComputedStyle(body).height;

        // Record which of four primary button was shown for the first screen.
        recordEvent("show", primary.dataset.l10nId);

        // Indicate to SubDialog that we're done sizing the first screen.
        ready();
      }

      // Let testing know the screen is ready to continue.
      dispatchEvent(new CustomEvent("ready"));
    });

    // Record that the screen was shown.
    recordEvent("show", current);
  })();
}
document.mozSubdialogReady = new Promise(resolve =>
  document.addEventListener("DOMContentLoaded", () => onLoad(resolve), {
    once: true,
  })
);
