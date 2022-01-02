"use strict";

{
    function getMessage(messageName, substitutions) {
        return chrome.i18n.getMessage(messageName, substitutions)
    }
    chrome.i18n.translateDocument = function (root = document) {
        for (const element of root.querySelectorAll("[data-i18n]")) {
            let text = getMessage(element.getAttribute("data-i18n"), element.getAttribute("data-i18n-ph-value"))
            if (!text) {
                continue;
            }

            element.textContent = text
        }

        for (const element of root.querySelectorAll("[data-i18n-title]")) {
            let text = getMessage(element.getAttribute("data-i18n-title"), element.getAttribute("data-i18n-ph-value"))
            if (!text) {
                continue;
            }

            element.setAttribute("title", text)
        }

        for (const element of root.querySelectorAll("[data-i18n-placeholder]")) {
            let text = getMessage(element.getAttribute("data-i18n-placeholder"), element.getAttribute("data-i18n-ph-value"))
            if (!text) {
                continue;
            }

            element.setAttribute("placeholder", text)
        }
    }

    if (chrome.tabs) { // 
        chrome.i18n.translateDocument()
    }
}