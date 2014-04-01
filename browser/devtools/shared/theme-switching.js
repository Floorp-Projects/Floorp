/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function() {
  const DEVTOOLS_SKIN_URL = "chrome://browser/skin/devtools/";

  function forceStyle() {
    let computedStyle = window.getComputedStyle(document.documentElement);
    if (!computedStyle) {
      // Null when documentElement is not ready. This method is anyways not
      // required then as scrollbars would be in their state without flushing.
      return;
    }
    let display = computedStyle.display; // Save display value
    document.documentElement.style.display = "none";
    window.getComputedStyle(document.documentElement).display; // Flush
    document.documentElement.style.display = display; // Restore
  }

  function switchTheme(newTheme, oldTheme) {
    if (newTheme === oldTheme) {
      return;
    }

    if (oldTheme && newTheme != oldTheme) {
      StylesheetUtils.removeSheet(
        window,
        DEVTOOLS_SKIN_URL + oldTheme + "-theme.css",
        "author"
      );
    }

    StylesheetUtils.loadSheet(
      window,
      DEVTOOLS_SKIN_URL + newTheme + "-theme.css",
      "author"
    );

    // Floating scrollbars à la osx
    let hiddenDOMWindow = Cc["@mozilla.org/appshell/appShellService;1"]
                 .getService(Ci.nsIAppShellService)
                 .hiddenDOMWindow;
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

    document.documentElement.classList.remove("theme-" + oldTheme);
    document.documentElement.classList.add("theme-" + newTheme);
  }

  function handlePrefChange(event, data) {
    if (data.pref == "devtools.theme") {
      switchTheme(data.newValue, data.oldValue);
    }
  }

  const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

  Cu.import("resource://gre/modules/Services.jsm");
  Cu.import("resource:///modules/devtools/gDevTools.jsm");
  const {devtools} = Components.utils.import("resource://gre/modules/devtools/Loader.jsm", {});
  const StylesheetUtils = devtools.require("sdk/stylesheet/utils");

  let theme = Services.prefs.getCharPref("devtools.theme");
  switchTheme("light");

  gDevTools.on("pref-changed", handlePrefChange);
  window.addEventListener("unload", function() {
    gDevTools.off("pref-changed", handlePrefChange);
  });
})();
