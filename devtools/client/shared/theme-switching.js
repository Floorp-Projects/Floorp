/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";
(function () {
  const { utils: Cu } = Components;
  const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
  const Services = require("Services");
  const { gDevTools } = require("devtools/client/framework/devtools");
  const { appendStyleSheet } = require("devtools/client/shared/stylesheet-utils");

  let documentElement = document.documentElement;

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

  // no-theme attributes allows to just est the platform attribute
  // to have per-platform CSS working correctly.
  if (documentElement.getAttribute("no-theme") === "true") {
    return;
  }

  let devtoolsStyleSheets = new WeakMap();
  let gOldTheme = "";

  function forceStyle() {
    let computedStyle = window.getComputedStyle(documentElement);
    if (!computedStyle) {
      // Null when documentElement is not ready. This method is anyways not
      // required then as scrollbars would be in their state without flushing.
      return;
    }
    // Save display value
    let display = computedStyle.display;
    documentElement.style.display = "none";
    // Flush
    window.getComputedStyle(documentElement).display;
    // Restore
    documentElement.style.display = display;
  }

  /*
   * Notify the window that a theme switch finished so tests can check the DOM
   */
  function notifyWindow() {
    window.dispatchEvent(new CustomEvent("theme-switch-complete", {}));
  }

  /*
   * Apply all the sheets from `newTheme` and remove all of the sheets
   * from `oldTheme`
   */
  function switchTheme(newTheme) {
    if (newTheme === gOldTheme) {
      return;
    }
    let oldTheme = gOldTheme;
    gOldTheme = newTheme;

    let oldThemeDef = gDevTools.getThemeDefinition(oldTheme);
    let newThemeDef = gDevTools.getThemeDefinition(newTheme);

    // The theme might not be available anymore (e.g. uninstalled)
    // Use the default one.
    if (!newThemeDef) {
      newThemeDef = gDevTools.getThemeDefinition("light");
    }

    // Store the sheets in a WeakMap for access later when the theme gets
    // unapplied.  It's hard to query for processing instructions so this
    // is an easy way to access them later without storing a property on
    // the window
    devtoolsStyleSheets.set(newThemeDef, []);

    let loadEvents = [];
    for (let url of newThemeDef.stylesheets) {
      let {styleSheet, loadPromise} = appendStyleSheet(document, url);
      devtoolsStyleSheets.get(newThemeDef).push(styleSheet);
      loadEvents.push(loadPromise);
    }

    try {
      const StylesheetUtils = require("devtools/shared/layout/utils");
      const SCROLLBARS_URL = "chrome://devtools/skin/floating-scrollbars-dark-theme.css";

      // TODO: extensions might want to customize scrollbar styles too.
      if (!Services.appShell.hiddenDOMWindow
        .matchMedia("(-moz-overlay-scrollbars)").matches) {
        if (newTheme == "dark") {
          StylesheetUtils.loadSheet(window, SCROLLBARS_URL, "agent");
        } else if (oldTheme == "dark") {
          StylesheetUtils.removeSheet(window, SCROLLBARS_URL, "agent");
        }
        forceStyle();
      }
    } catch (e) {
      console.warn("customize scrollbar styles is only supported in firefox");
    }

    Promise.all(loadEvents).then(() => {
      // Unload all stylesheets and classes from the old theme.
      if (oldThemeDef) {
        for (let name of oldThemeDef.classList) {
          documentElement.classList.remove(name);
        }

        for (let sheet of devtoolsStyleSheets.get(oldThemeDef) || []) {
          sheet.remove();
        }

        if (oldThemeDef.onUnapply) {
          oldThemeDef.onUnapply(window, newTheme);
        }
      }

      // Load all stylesheets and classes from the new theme.
      for (let name of newThemeDef.classList) {
        documentElement.classList.add(name);
      }

      if (newThemeDef.onApply) {
        newThemeDef.onApply(window, oldTheme);
      }

      // Final notification for further theme-switching related logic.
      gDevTools.emit("theme-switched", window, newTheme, oldTheme);
      notifyWindow();
    }, console.error.bind(console));
  }

  function handlePrefChange() {
    switchTheme(Services.prefs.getCharPref("devtools.theme"));
  }

  if (documentElement.hasAttribute("force-theme")) {
    switchTheme(documentElement.getAttribute("force-theme"));
  } else {
    switchTheme(Services.prefs.getCharPref("devtools.theme"));

    Services.prefs.addObserver("devtools.theme", handlePrefChange);
    window.addEventListener("unload", function () {
      Services.prefs.removeObserver("devtools.theme", handlePrefChange);
    }, { once: true });
  }
})();
