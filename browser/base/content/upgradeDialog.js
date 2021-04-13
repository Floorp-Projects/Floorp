/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  AddonManager,
  document: gDoc,
  getShellService,
  Services,
} = window.docShell.chromeEventHandler.ownerGlobal;

const SHELL = getShellService();
const IS_DEFAULT = SHELL.isDefaultBrowser();
const CAN_PIN = (() => {
  try {
    SHELL.QueryInterface(
      Ci.nsIWindowsShellService
    ).checkPinCurrentAppToTaskbar();
    return true;
  } catch (ex) {
    return false;
  }
})();
const NEED_PIN = Promise.resolve(
  CAN_PIN && SHELL.isCurrentAppPinnedToTaskbarAsync().then(pinned => !pinned)
);

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

  // Assuming there's always one active theme, this won't happen.
  return {};
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

// Hook up dynamic behaviors of the dialog.
function onLoad(ready) {
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
        break;

      case 1:
        // Handle first screen conditional actions.
        if (target === primary) {
          if (!IS_DEFAULT) {
            SHELL.setAsDefault();
          }
          if (await NEED_PIN) {
            try {
              SHELL.pinCurrentAppToTaskbar();
            } catch (ex) {}
          }
        } else if (target === secondary && IS_DEFAULT && !(await NEED_PIN)) {
          closeDialog("early");
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

    // Ensure the primary button is focused on each screen.
    requestAnimationFrame(() => {
      primary.focus();

      // Save first screen height, so later screens can flex and anchor content.
      if (current === 0) {
        document.body.style.minHeight = getComputedStyle(document.body).height;

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
