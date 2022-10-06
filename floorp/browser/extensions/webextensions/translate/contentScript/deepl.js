"use strict";

{
    function translate(text, targetlanguage) {
        return new Promise(resolve => {
            const source_textarea = document.querySelector(".lmt__source_textarea")
            const target_textarea = document.querySelector(".lmt__target_textarea")

            try {
                const deepl_currentTargetLanguage = document.querySelector(`div[dl-test="translator-target-lang"]`).getAttribute("dl-selected-lang")
                targetlanguage = targetlanguage.split("-")[0]
                if (deepl_currentTargetLanguage && deepl_currentTargetLanguage.split("-")[0].toLowerCase() !== targetlanguage) {
                    document.querySelector(`button[dl-test="translator-target-lang-btn"]`).click()
                    document.querySelector(`button[dl-test|="translator-lang-option-${targetlanguage}"]`).click()
                } else if (target_textarea.value && text === source_textarea.value) {
                    resolve(target_textarea.value)
                    return
                }
            } catch (e) {
                console.error(e)
            }

            const event = new Event("change")

            source_textarea.value = text
            source_textarea.dispatchEvent(event)

            const startTime = performance.now()

            function checkresult(oldvalue) {
                if ((performance.now() - startTime) > 2400 || (target_textarea.value && target_textarea.value !== oldvalue)) {
                    resolve(target_textarea.value)
                    return
                }
                setTimeout(checkresult, 100, oldvalue)
            }
            checkresult(target_textarea.value)
        })
    }

    if (location.hash.startsWith("#!")) {
        let [targetLanguage, text] = location.hash.split("!#")
        location.hash = ""

        targetLanguage = decodeURIComponent(targetLanguage.substring(2))
        text = decodeURIComponent(text)

        translate(text, targetLanguage || "en")
            .then(result => {
                console.log(result)
                chrome.runtime.sendMessage({
                    action: "DeepL_firstTranslationResult",
                    result
                })
            })
    }

    chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
        if (request.action === "translateTextWithDeepL") {
            translate(request.text, request.targetlanguage)
                .then(result => {
                    console.log(result)
                    sendResponse(result)
                })
        }

        return true
    })
}