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
const COMPACT_MODE_HEIGHT = 649;

const SHELL = getShellService();
const IS_DEFAULT = SHELL.isDefaultBrowser();
const NEED_PIN = SHELL.doesAppNeedPin();

// Strings for various elements with matching ids on each screen.
const SCREEN_STRINGS = [
  NEED_PIN.then(pin =>
    pin
      ? {
          title: "upgrade-dialog-pin-title",
          subtitle: "upgrade-dialog-pin-subtitle",
          primary: "upgrade-dialog-pin-primary-button",
          secondary: "upgrade-dialog-pin-secondary-button",
        }
      : {
          title: "upgrade-dialog-new-title",
          subtitle: "upgrade-dialog-new-subtitle",
          primary: IS_DEFAULT
            ? "upgrade-dialog-new-primary-theme-button"
            : "upgrade-dialog-new-primary-default-button",
          secondary: "upgrade-dialog-new-secondary-button",
        }
  ),
  {
    title: "upgrade-dialog-default-title-2",
    subtitle: "upgrade-dialog-default-subtitle-2",
    primary: "upgrade-dialog-default-primary-button-2",
    secondary: "upgrade-dialog-default-secondary-button",
  },
  {
    title: "upgrade-dialog-theme-title-2",
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

  const { body } = document;
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
    const strings = await SCREEN_STRINGS[++current];
    switch (current) {
      // Handle initial / first screen setup.
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

          if (IS_DEFAULT) {
            strings.primary = "upgrade-dialog-new-primary-win7-button";
            secondary.style.display = "none";
          }
        }

        // Avoid distracting the user with items for "pin" content.
        let removeDefaultScreen = true;
        if (await NEED_PIN) {
          items.remove();
          removeDefaultScreen = IS_DEFAULT;
        }

        // Keep the second screen only if we need to both pin and set default.
        if (removeDefaultScreen) {
          SCREEN_STRINGS.splice(1, 1);
        }

        // NB: We keep the screen count for win7 at 2, so actions are handled in
        // "default" case. Send telemetry to be able to identify what users see.
        recordEvent("show", `${SCREEN_STRINGS.length}-screens`);

        // Copy the initial step indicator enough times for each screen.
        for (let i = SCREEN_STRINGS.length; i > 1; i--) {
          steps.append(steps.lastChild.cloneNode(true));
        }
        steps.lastChild.classList.add("current");
        break;

      // Handle actions and setup for not-first and not-last screens.
      default:
        const { l10nId } = primary.dataset;
        if (target === primary) {
          switch (l10nId) {
            case "upgrade-dialog-new-primary-default-button":
            case "upgrade-dialog-default-primary-button-2":
              SHELL.setAsDefault();
              break;
            case "upgrade-dialog-pin-primary-button":
              SHELL.pinToTaskbar();
              break;
          }
        } else if (
          target === secondary &&
          l10nId === "upgrade-dialog-new-primary-theme-button"
        ) {
          closeDialog("early");
          return;
        }

        // First screen is the only screen for Windows 7.
        if (win7Content) {
          closeDialog("win7");
          return;
        }

        // Prepare theme screen content only when we're moving to the last one.
        if (current !== SCREEN_STRINGS.length - 1) {
          break;
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
        adjustModalBackdrop();
        break;

      // Handle the last (theme) screen actions.
      case SCREEN_STRINGS.length:
        // New theme is confirmed, so don't revert to previous.
        if (target === primary) {
          gPrevTheme = null;
        }

        closeDialog("complete");
        return;
    }

    // Update various elements reused across screens.
    steps.prepend(steps.lastChild);

    // Update strings for reused elements that change between screens.
    await document.l10n.ready;
    const translatedElements = [];
    for (let el of [title, subtitle, primary, secondary]) {
      const stringId = strings[el.id];
      if (stringId) {
        document.l10n.setAttributes(el, stringId);
        translatedElements.push(el);
      }
    }

    // Wait for initial translations to load before getting sizing information.
    await document.l10n.translateElements(translatedElements);
    requestAnimationFrame(() => {
      // Ensure the primary button is focused on each screen.
      primary.focus({ preventFocusRing: true });

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
  })();
}
document.mozSubdialogReady = new Promise(resolve =>
  document.addEventListener("DOMContentLoaded", () => onLoad(resolve), {
    once: true,
  })
);
