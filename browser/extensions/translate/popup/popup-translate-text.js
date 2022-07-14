"use strict";

var $ = document.querySelector.bind(document)

twpConfig.onReady(function () {
    function backgroundTranslateSingleText(translationService, targetLanguage, source) {
        return new Promise((resolve, reject) => {
            chrome.runtime.sendMessage({
                action: "translateSingleText",
                translationService,
                targetLanguage,
                source
            }, response => {
                resolve(response)
            })
        })
    }

    let currentTargetLanguages = twpConfig.get("targetLanguages")
    let currentTargetLanguage = twpConfig.get("targetLanguageTextTranslation")
    let currentTextTranslatorService = twpConfig.get("textTranslatorService")

    function enableDarkMode() {
        if (!document.getElementById("darkModeElement")) {
            const el = document.createElement("style")
            el.setAttribute("id", "darkModeElement")
            el.setAttribute("rel", "stylesheet")
            el.textContent = `
            * {
                scrollbar-color: #202324 #454a4d;
            }
            body {
                color: rgb(231, 230, 228) !important;
                background-color: #181a1b !important;
            }
            hr {
                border-color: #666;
            }
            li:hover {
                color: rgb(231, 230, 228) !important;
                background-color: #454a4d !important;
            }
            .selected {
                background-color: #454a4d !important;
            }
            `
            document.body.appendChild(el)
            document.querySelector("#listen svg").style = "fill: rgb(231, 230, 228)"
        }
    }

    function disableDarkMode() {
        if (document.getElementById("#darkModeElement")) {
            document.getElementById("#darkModeElement").remove()
            document.querySelector("#listen svg").style = "fill: black"
        }
    }

    switch (twpConfig.get("darkMode")) {
        case "auto":
            if (matchMedia("(prefers-color-scheme: dark)").matches) {
                enableDarkMode()
            } else {
                disableDarkMode()
            }
            break
        case "yes":
            enableDarkMode()
            break
        case "no":
            disableDarkMode()
            break
        default:
            break
    }

    let audioDataUrls = null
    let isPlayingAudio = false

    function stopAudio() {
        if (isPlayingAudio) {
            chrome.runtime.sendMessage({
                action: "stopAudio"
            })
        }
        isPlayingAudio = false
    }


    const eOrigText = document.getElementById("eOrigText")
    const eOrigTextDiv = document.getElementById("eOrigTextDiv")
    const eTextTranslated = document.getElementById("eTextTranslated")

    const sGoogle = document.getElementById("sGoogle")
    const sYandex = document.getElementById("sYandex")
    const sBing = document.getElementById("sBing")
    const sDeepL = document.getElementById("sDeepL")

    function setCaretAtEnd() {
        const el = eOrigText
        const range = document.createRange()
        const sel = window.getSelection()
        range.setStart(el, 1)
        range.collapse(true)
        sel.removeAllRanges()
        sel.addRange(range)
        el.focus()
    }

    let translateNewInputTimerHandler
    eOrigText.oninput = () => {
        clearTimeout(translateNewInputTimerHandler)
        translateNewInputTimerHandler = setTimeout(translateText, 800)
    }

    sGoogle.onclick = () => {
        currentTextTranslatorService = "google"
        twpConfig.set("textTranslatorService", "google")
        translateText()

        sGoogle.classList.remove("selected")
        sYandex.classList.remove("selected")
        sBing.classList.remove("selected")
        sDeepL.classList.remove("selected")

        sGoogle.classList.add("selected")
    }
    sYandex.onclick = () => {
        currentTextTranslatorService = "yandex"
        twpConfig.set("textTranslatorService", "yandex")
        translateText()

        sGoogle.classList.remove("selected")
        sYandex.classList.remove("selected")
        sBing.classList.remove("selected")
        sDeepL.classList.remove("selected")

        sYandex.classList.add("selected")
    }
    sBing.onclick = () => {
        currentTextTranslatorService = "bing"
        twpConfig.set("textTranslatorService", "bing")
        translateText()

        sGoogle.classList.remove("selected")
        sYandex.classList.remove("selected")
        sBing.classList.remove("selected")
        sDeepL.classList.remove("selected")

        sBing.classList.add("selected")
    }
    sDeepL.onclick = () => {
        currentTextTranslatorService = "deepl"
        twpConfig.set("textTranslatorService", "deepl")
        translateText()

        sGoogle.classList.remove("selected")
        sYandex.classList.remove("selected")
        sBing.classList.remove("selected")
        sDeepL.classList.remove("selected")

        sDeepL.classList.add("selected")
    }

    const setTargetLanguage = document.getElementById("setTargetLanguage")
    setTargetLanguage.onclick = e => {
        if (e.target.getAttribute("value")) {
            const langCode = twpLang.checkLanguageCode(e.target.getAttribute("value"))
            if (langCode) {
                currentTargetLanguage = langCode
                twpConfig.setTargetLanguageTextTranslation(langCode)
                translateText()
            }

            document.querySelectorAll("#setTargetLanguage li").forEach(li => {
                li.classList.remove("selected")
            })

            e.target.classList.add("selected")
        }
    }

    const eListen = document.getElementById("listen")
    eListen.classList.remove("selected")
    eListen.onclick = () => {
        const msgListen = chrome.i18n.getMessage("btnListen")
        const msgStopListening = chrome.i18n.getMessage("btnStopListening")

        eListen.classList.remove("selected")
        eListen.setAttribute("title", msgStopListening)

        if (audioDataUrls) {
            if (isPlayingAudio) {
                stopAudio()
                eListen.setAttribute("title", msgListen)
            } else {
                isPlayingAudio = true
                chrome.runtime.sendMessage({
                    action: "playAudio",
                    audioDataUrls
                }, () => {
                    eListen.classList.remove("selected")
                    eListen.setAttribute("title", msgListen)
                })
                eListen.classList.add("selected")
            }
        } else {
            stopAudio()
            isPlayingAudio = true
            chrome.runtime.sendMessage({
                action: "textToSpeech",
                text: eTextTranslated.textContent,
                targetLanguage: currentTargetLanguage
            }, result => {
                if (!result) return;

                audioDataUrls = result
                chrome.runtime.sendMessage({
                    action: "playAudio",
                    audioDataUrls
                }, () => {
                    isPlayingAudio = false
                    eListen.classList.remove("selected")
                    eListen.setAttribute("title", msgListen)
                })
            })
            eListen.classList.add("selected")
        }
    }

    const targetLanguageButtons = document.querySelectorAll("#setTargetLanguage li")

    for (let i = 0; i < 3; i++) {
        if (currentTargetLanguages[i] == currentTargetLanguage) {
            targetLanguageButtons[i].classList.add("selected")
        }
        targetLanguageButtons[i].textContent = currentTargetLanguages[i]
        targetLanguageButtons[i].setAttribute("value", currentTargetLanguages[i])
        targetLanguageButtons[i].setAttribute("title", twpLang.codeToLanguage(currentTargetLanguages[i]))
    }

    if (currentTextTranslatorService === "yandex") {
        sYandex.classList.add("selected")
    } else if (currentTextTranslatorService == "deepl") {
        sDeepL.classList.add("selected")
    } else if (currentTextTranslatorService == "bing") {
        sBing.classList.add("selected")
    } else {
        sGoogle.classList.add("selected")
    }

    if (twpConfig.get("enableDeepL") === "yes") {
        sDeepL.removeAttribute("hidden")
    } else {
        sDeepL.setAttribute("hidden", "")
    }
    twpConfig.onChanged((name, newvalue) => {
        switch (name) {
            case "enableDeepL":
                if (newvalue === "yes") {
                    sDeepL.removeAttribute("hidden")
                } else {
                    sDeepL.setAttribute("hidden", "")
                }
                break
        }
    })


    function translateText() {
        stopAudio()
        audioDataUrls = null

        backgroundTranslateSingleText(currentTextTranslatorService, currentTargetLanguage, eOrigText.textContent)
            .then(result => {
                if (twpLang.isRtlLanguage(currentTargetLanguage)) {
                    eTextTranslated.setAttribute("dir", "rtl")
                } else {
                    eTextTranslated.setAttribute("dir", "ltr")
                }
                eTextTranslated.textContent = result
            })
    }

    let params = location.hash.substring(1).split("&").reduce((a, b) => {
        const splited = b.split("=")
        a[decodeURIComponent(splited[0])] = decodeURIComponent(splited[1])
        return a
    }, {})

    if (params["text"]) {
        eOrigText.textContent = params["text"]
        setCaretAtEnd()
        translateText()
    }
})