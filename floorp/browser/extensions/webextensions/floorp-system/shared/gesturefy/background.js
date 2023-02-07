browser.runtime.onMessage.addListener(async (val) => {
return await browser.browserL10n.getFloorpL10nValues(val)
})