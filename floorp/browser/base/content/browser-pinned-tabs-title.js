{
    let apply = function() {
        document.getElementById("showPinnedTabsTitle-css")?.remove();
        let enabled = Services.prefs.getBoolPref("floorp.tabs.showPinnedTabsTitle", false);
        if (enabled) {
            let tabMinWidth = Services.prefs.getIntPref("browser.tabs.tabMinWidth");
            let css = document.createElement("style");
            css.id = "showPinnedTabsTitle-css";
            css.textContent = `
            .tab-label-container[pinned] {
                width: unset !important;
            }
            .tabbrowser-tab[pinned="true"] {
                width: ${tabMinWidth}px !important;
            }`;
            document.body.appendChild(css);
        }
        let tabBrowserTabs = document.getElementById("tabbrowser-tabs");
        tabBrowserTabs._pinnedTabsLayoutCache = null;
        tabBrowserTabs._positionPinnedTabs();
    }
    Services.prefs.addObserver("browser.tabs.tabMinWidth", apply);
    Services.prefs.addObserver("browser.tabs.tabMinWidth", apply);
    apply();
}
