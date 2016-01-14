/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function() {
  const SCROLLBARS_URL = "chrome://devtools/skin/floating-scrollbars-light.css";
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

  /*
   * Append a new processing instruction and return an object with
   *  - styleSheet: DOMNode
   *  - loadPromise: Promise that resolves once the sheets loads or errors
   */
  function appendStyleSheet(url) {
    let styleSheetAttr = `href="${url}" type="text/css"`;
    let styleSheet = document.createProcessingInstruction(
      "xml-stylesheet", styleSheetAttr);
    let loadPromise = new Promise((resolve, reject) => {
      function onload() {
        styleSheet.removeEventListener("load", onload);
        styleSheet.removeEventListener("error", onerror);
        resolve();
      }
      function onerror() {
        styleSheet.removeEventListener("load", onload);
        styleSheet.removeEventListener("error", onerror);
        reject("Failed to load theme file " + url);
      }

      styleSheet.addEventListener("load", onload);
      styleSheet.addEventListener("error", onerror);
    });
    document.insertBefore(styleSheet, documentElement);
    return {styleSheet, loadPromise};
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
  function switchTheme(newTheme, oldTheme) {
    if (newTheme === oldTheme) {
      return;
    }

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
      let {styleSheet,loadPromise} = appendStyleSheet(url);
      devtoolsStyleSheets.get(newThemeDef).push(styleSheet);
      loadEvents.push(loadPromise);
    }

    // Floating scroll-bars like in OSX
    let hiddenDOMWindow = Cc["@mozilla.org/appshell/appShellService;1"]
                 .getService(Ci.nsIAppShellService)
                 .hiddenDOMWindow;

    // TODO: extensions might want to customize scrollbar styles too.
    if (!hiddenDOMWindow.matchMedia("(-moz-overlay-scrollbars)").matches) {
      if (newTheme == "dark") {
        StylesheetUtils.loadSheet(window, SCROLLBARS_URL, "agent");
      } else if (oldTheme == "dark") {
        StylesheetUtils.removeSheet(window, SCROLLBARS_URL, "agent");
      }
      forceStyle();
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
