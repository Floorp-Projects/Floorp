/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function() {
  const DEVTOOLS_SKIN_URL = "chrome://devtools/skin/";
  let documentElement = document.documentElement;

  function forceStyle() {
    let computedStyle = window.getComputedStyle(documentElement);
    if (!computedStyle) {
      // Null when documentElement is not ready. This method is anyways not
      // required then as scrollbars would be in their state without flushing.
      return;
    }
    let display = computedStyle.display; // Save display value
    documentElement.style.display = "none";
    window.getComputedStyle(documentElement).display; // Flush
    documentElement.style.display = display; // Restore
  }

  function switchTheme(newTheme, oldTheme) {
    if (newTheme === oldTheme) {
      return;
    }

    let oldThemeDef = gDevTools.getThemeDefinition(oldTheme);

    // Unload all theme stylesheets related to the old theme.
    if (oldThemeDef) {
      for (let url of oldThemeDef.stylesheets) {
        StylesheetUtils.removeSheet(window, url, "author");
      }
    }

    // Load all stylesheets associated with the new theme.
    let newThemeDef = gDevTools.getThemeDefinition(newTheme);

    // The theme might not be available anymore (e.g. uninstalled)
    // Use the default one.
    if (!newThemeDef) {
      newThemeDef = gDevTools.getThemeDefinition("light");
    }

    for (let url of newThemeDef.stylesheets) {
      StylesheetUtils.loadSheet(window, url, "author");
    }

    // Floating scroll-bars like in OSX
    let hiddenDOMWindow = Cc["@mozilla.org/appshell/appShellService;1"]
                 .getService(Ci.nsIAppShellService)
                 .hiddenDOMWindow;

    // TODO: extensions might want to customize scrollbar styles too.
    if (!hiddenDOMWindow.matchMedia("(-moz-overlay-scrollbars)").matches) {
      let scrollbarsUrl = Services.io.newURI(
        DEVTOOLS_SKIN_URL + "floating-scrollbars-light.css", null, null);

      if (newTheme == "dark") {
        StylesheetUtils.loadSheet(
          window,
          scrollbarsUrl,
          "agent"
        );
      } else if (oldTheme == "dark") {
        StylesheetUtils.removeSheet(
          window,
          scrollbarsUrl,
          "agent"
        );
      }
      forceStyle();
    }

    if (oldThemeDef) {
      for (let name of oldThemeDef.classList) {
        documentElement.classList.remove(name);
      }

      if (oldThemeDef.onUnapply) {
        oldThemeDef.onUnapply(window, newTheme);
      }
    }

    for (let name of newThemeDef.classList) {
      documentElement.classList.add(name);
    }

    if (newThemeDef.onApply) {
      newThemeDef.onApply(window, oldTheme);
    }

    // Final notification for further theme-switching related logic.
    gDevTools.emit("theme-switched", window, newTheme, oldTheme);
  }

  function handlePrefChange(event, data) {
    if (data.pref == "devtools.theme") {
      switchTheme(data.newValue, data.oldValue);
    }
  }

  const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

  Cu.import("resource://gre/modules/Services.jsm");
  Cu.import("resource://devtools/client/framework/gDevTools.jsm");
  const {require} = Components.utils.import("resource://devtools/shared/Loader.jsm", {});
  const StylesheetUtils = require("sdk/stylesheet/utils");

  let os;
  let platform = navigator.platform;
  if (platform.startsWith("Win")) {
    os = "win";
  } else if (platform.startsWith("Mac")) {
    os = "mac";
  } else {
    os = "linux";
  }
  documentElement.setAttribute("platform", os);

  if (documentElement.hasAttribute("force-theme")) {
    switchTheme(documentElement.getAttribute("force-theme"));
  } else {
    switchTheme(Services.prefs.getCharPref("devtools.theme"));

    gDevTools.on("pref-changed", handlePrefChange);
    window.addEventListener("unload", function() {
      gDevTools.off("pref-changed", handlePrefChange);
    });
  }
})();
