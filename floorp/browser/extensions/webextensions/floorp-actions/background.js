async function handle_actions(action, options) {
    console.log(action);
    switch (action) {
        case "open-extension-sidebar":
            if (!options.extensionId) throw '"extensionId" must be specified.';
            var widgetId = await browser.floorpActions.getExtensionWidgetId(options.extensionId);
            await browser.floorpActions.openInSidebar(`${widgetId}-sidebar-action`);
            break;
        case "open-bookmarks-sidebar":
            await browser.floorpActions.openInSidebar("viewBookmarksSidebar");
            break;
        case "open-history-sidebar":
            await browser.floorpActions.openInSidebar("viewHistorySidebar");
            break;
        case "open-synctabs-sidebar":
            await browser.floorpActions.openInSidebar("viewTabsSidebar");
            break;
        case "close-sidebar":
            await browser.floorpActions.closeSidebar();
            break;
        case "open-browser-manager-sidebar":
            await browser.floorpActions.openBrowserManagerSidebar();
            break;
        case "close-browser-manager-sidebar":
            await browser.floorpActions.closeBrowserManagerSidebar();
            break;
        case "toggle-browser-manager-sidebar":
            await browser.floorpActions.changeBrowserManagerSidebarVisibility();
            break;
        case "show-statusbar":
            await browser.floorpActions.showStatusbar();
            break;
        case "hide-statusbar":
            await browser.floorpActions.hideStatusbar();
            break;
        case "toggle-statusbar":
            await browser.floorpActions.toggleStatusbar();
            break;
    }
}

