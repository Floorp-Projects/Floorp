browser.runtime.onMessage.addListener(async (val) => {
    switch(val.type){
        case "l10n":
            return await browser.browserL10n.getFloorpL10nValues(val.data)
        case "extensionsInSidebar":
            return await browser.sidebar.getExtensionsInSidebar()
    }

})