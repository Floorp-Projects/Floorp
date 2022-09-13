"use strict";

{
    function getMessage(messageName, substitutions) {
        return chrome.i18n.getMessage(messageName, substitutions)
    }

    function translateAttributes(root, attributeName) {
        for (const element of root.querySelectorAll(`[data-i18n-${attributeName}]`)) {
            let text = getMessage(element.getAttribute(`data-i18n-${attributeName}`), element.getAttribute("data-i18n-ph-value"))
            if (!text) {
                continue;
            }

            element.setAttribute(attributeName, text)
        }
    }

    chrome.i18n.translateDocument = function (root = document) {
        for (const element of root.querySelectorAll("[data-i18n]")) {
            let text = getMessage(element.getAttribute("data-i18n"), element.getAttribute("data-i18n-ph-value"))
            if (!text) {
                continue;
            }
            element.textContent = text
        }

        translateAttributes(root, "title")
        translateAttributes(root, "placeholder")
        translateAttributes(root, "label")
    }

    if (chrome.tabs) { // 
        chrome.i18n.translateDocument()
    }
}