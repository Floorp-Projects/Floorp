chrome.tabs.query({active: true, currentWindow: true}, tabs => {
    if (tabs[0].url.toLowerCase().endsWith(".pdf")) {
        window.location = "popup-translate-document.html"
    }
})

chrome.runtime.sendMessage({action: "getTabMimeType"}, mimeType => {
    if (mimeType && mimeType.toLowerCase() === "application/pdf") {
        window.location = "popup-translate-document.html"
    }
})