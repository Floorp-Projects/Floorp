"use strict";

let $ = document.querySelector.bind(document);

const selectTargetLanguage = $("select")

$("#btnClose").addEventListener("click", () => {
    window.location = "popup.html"
})

$("#btnApply").addEventListener("click", () => {
    twpConfig.setTargetLanguage(selectTargetLanguage.value, true)
    window.location = "popup.html"
})

twpConfig.onReady(function () {
    let uilanguage = chrome.i18n.getUILanguage()
    uilanguage = twpLang.fixLanguageCode(uilanguage)

    let langs = twpLang.languages[uilanguage]
    if (!langs) {
        langs = twpLang.languages["en"]
    }

    const langsSorted = []

    for (const i in langs) {
        langsSorted.push([i, langs[i]])
    }

    langsSorted.sort(function (a, b) {
        return a[1].localeCompare(b[1]);
    })

    const eAllLangs = selectTargetLanguage.querySelector('[name="all"]')
    langsSorted.forEach(value => {
        if (value[0] == "zh" || value[0] == "un" || value[0] == "und") return;
        const option = document.createElement("option")
        option.value = value[0]
        option.textContent = value[1]
        eAllLangs.appendChild(option)
    })
    
    const eRecentsLangs = selectTargetLanguage.querySelector('[name="targets"]')
    for (const value of twpConfig.get("targetLanguages")) {
        const option = document.createElement("option")
        option.value = value
        option.textContent = langs[value]
        eRecentsLangs.appendChild(option)
    }
    selectTargetLanguage.value = twpConfig.get("targetLanguages")[0]

    function disableDarkMode() {
        if (!$("#lightModeElement")) {
            const el = document.createElement("style")
            el.setAttribute("id", "lightModeElement")
            el.setAttribute("rel", "stylesheet")
            el.textContent = `
            body {
                color: rgb(0, 0, 0);
                background-color: rgb(224, 224, 224);
            }

            .select, #btnApply {
                color: black;
                background-color: rgba(0, 0, 0, 0.2);
            }
            
            .select:hover, #btnApply:hover {
                background-color: rgba(0, 0, 0, 0.4);
            }

            .mdiv, .md {
                background-color: rgb(0, 0, 0);
            }
            `
            document.head.appendChild(el)
        }
    }

    function enableDarkMode() {
        if ($("#lightModeElement")) {
            $("#lightModeElement").remove()
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
})
