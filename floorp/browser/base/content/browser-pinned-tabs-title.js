/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

{
  let apply = () => {
    const showPinnedTabsTitleCSS = document.getElementById(
      "showPinnedTabsTitle-css"
    );
    if (showPinnedTabsTitleCSS) {
      showPinnedTabsTitleCSS.remove();
    }

    const enabled = Services.prefs.getBoolPref(
      "floorp.tabs.showPinnedTabsTitle",
      false
    );
    if (enabled) {
      const tabMinWidth = Services.prefs.getIntPref("browser.tabs.tabMinWidth");
      const css = document.createElement("style");
      css.id = "showPinnedTabsTitle-css";
      css.textContent = `
          .tab-label-container[pinned] {
            width: unset !important;
          }
          .tabbrowser-tab[pinned="true"] {
            width: ${tabMinWidth}px !important;
          }
          .tab-throbber[pinned], .tab-icon-pending[pinned], .tab-icon-image[pinned], .tab-sharing-icon-overlay[pinned], .tab-icon-overlay[pinned] {
            margin-inline-end: 5.5px !important;
          }`;
      document.body.appendChild(css);
    }

    setTimeout(() => {
      const tabBrowserTabs = document.getElementById("tabbrowser-tabs");
      tabBrowserTabs._pinnedTabsLayoutCache = null;
      tabBrowserTabs._positionPinnedTabs();
    }, 100);
  };

  Services.prefs.addObserver("floorp.tabs.showPinnedTabsTitle", apply);
  Services.prefs.addObserver("browser.tabs.tabMinWidth", apply);
  apply();
}
