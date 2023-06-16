/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";
(function () {
  const { require } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/Loader.sys.mjs"
  );
  const {
    gDevTools,
  } = require("resource://devtools/client/framework/devtools.js");
  const {
    appendStyleSheet,
  } = require("resource://devtools/client/shared/stylesheet-utils.js");

  const {
    getTheme,
    getAutoTheme,
    addThemeObserver,
    removeThemeObserver,
  } = require("resource://devtools/client/shared/theme.js");

  const documentElement = document.documentElement;

  let os;
  const platform = navigator.platform;
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

  const devtoolsStyleSheets = new WeakMap();
  let gOldTheme = "";

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
    const oldTheme = gOldTheme;
    gOldTheme = newTheme;

    const oldThemeDef = gDevTools.getThemeDefinition(oldTheme);
    let newThemeDef = gDevTools.getThemeDefinition(newTheme);

    // The theme might not be available anymore (e.g. uninstalled)
    // Use the default one.
    if (!newThemeDef) {
      newThemeDef = gDevTools.getThemeDefinition(getAutoTheme());
    }

    // Store the sheets in a WeakMap for access later when the theme gets
    // unapplied.  It's hard to query for processing instructions so this
    // is an easy way to access them later without storing a property on
    // the window
    devtoolsStyleSheets.set(newThemeDef, []);

    const loadEvents = [];
    for (const url of newThemeDef.stylesheets) {
      const { styleSheet, loadPromise } = appendStyleSheet(document, url);
      devtoolsStyleSheets.get(newThemeDef).push(styleSheet);
      loadEvents.push(loadPromise);
    }

    Promise.all(loadEvents).then(() => {
      // Unload all stylesheets and classes from the old theme.
      if (oldThemeDef) {
        for (const name of oldThemeDef.classList) {
          documentElement.classList.remove(name);
        }

        for (const sheet of devtoolsStyleSheets.get(oldThemeDef) || []) {
          sheet.remove();
        }

        if (oldThemeDef.onUnapply) {
          oldThemeDef.onUnapply(window, newTheme);
        }
      }

      // Load all stylesheets and classes from the new theme.
      for (const name of newThemeDef.classList) {
        documentElement.classList.add(name);
      }

      if (newThemeDef.onApply) {
        newThemeDef.onApply(window, oldTheme);
      }

      // Final notification for further theme-switching related logic.
      gDevTools.emit("theme-switched", window, newTheme, oldTheme);
      notifyWindow();
    }, console.error);
  }

  function handleThemeChange() {
    switchTheme(getTheme());
  }

  // Check if the current document or the embedder of the document enforces a
  // theme.
  const forcedTheme =
    documentElement.getAttribute("force-theme") ||
    window.top.document.documentElement.getAttribute("force-theme");

  if (forcedTheme) {
    switchTheme(forcedTheme);
  } else {
    switchTheme(getTheme());

    addThemeObserver(handleThemeChange);
    window.addEventListener(
      "unload",
      function () {
        removeThemeObserver(handleThemeChange);
      },
      { once: true }
    );
  }
})();
