(async() => {
    let platform = (await browser.runtime.getPlatformInfo()).os;
    if (platform !== "mac") browser.browserAction.disable();
})()
