async function handle_actions(action, options) {
    console.log(action);
    switch (action) {
        case "open-tree-style-tab":
        case "open-extension-sidebar":
            let widgetId = ""
            if(action == "open-tree-style-tab"){
                widgetId = await browser.floorpActions.getExtensionWidgetId("treestyletab@piro.sakura.ne.jp");
            }else{
                if (!options.extensionId) throw '"extensionId" must be specified.';
                widgetId = await browser.floorpActions.getExtensionWidgetId(options.extensionId);
            }
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

function handleMessage(message, sender) {
    if (sender.id === "{506e023c-7f2b-40a3-8066-bc5deb40aebe}") {
        const example_obj = {
            "action": "tree-style-tab-open"
        }
        let message_obj =
            typeof message === "string" ?
                JSON.parse(message) :
                message
        handle_actions(
            message_obj["action"],
            message_obj["options"] ?
                message_obj["options"] :
                {}
        );
    }
}

browser.runtime.onMessageExternal.addListener(handleMessage);