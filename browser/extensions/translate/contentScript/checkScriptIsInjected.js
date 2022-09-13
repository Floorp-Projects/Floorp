chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request.action === "contentScriptIsInjected") {
        sendResponse(true)
    }
})