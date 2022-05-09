"use strict";

var $ = document.querySelector.bind(document)

twpConfig.onReady(function () {
    if (!navigator.userAgent.includes("Firefox")) {
        document.body.style.minWidth = "300px"
    }

    // get elements
    const btnSwitchInterfaces = document.getElementById("btnSwitchInterfaces")
    const divIconTranslateContainer = document.getElementById("divIconTranslateContainer")
    const divIconTranslate = document.getElementById("divIconTranslate")
    const iconTranslate = document.getElementById("iconTranslate")

    const lblTranslate = document.getElementById("lblTranslate")
    const lblTranslating = document.getElementById("lblTranslating")
    const lblTranslated = document.getElementById("lblTranslated")
    const lblError = document.getElementById("lblError")
    const lblTargetLanguage = document.getElementById("lblTargetLanguage")

    const divAlwaysTranslate = document.getElementById("divAlwaysTranslateThisLang")
    const cbAlwaysTranslate = document.getElementById("cbAlwaysTranslateThisLang")
    const lblAlwaysTranslate = document.getElementById("lblAlwaysTranslateThisLang")

    const selectTargetLanguage = document.getElementById("selectTargetLanguage")

    const divOptionsList = document.getElementById("divOptionsList")

    const btnReset = document.getElementById("btnReset")
    const btnTranslate = document.getElementById("btnTranslate")
    const btnRestore = document.getElementById("btnRestore")
    const btnTryAgain = document.getElementById("btnTryAgain")
    const btnOptionsDiv = document.getElementById("btnOptionsDiv")
    const btnOptions = document.getElementById("btnOptions")

    const divDonate = document.getElementById("divDonate")
    const msgHelpDevelopment = document.getElementById("msgHelpDevelopment")
    const btnDonateWithPaypal = document.getElementById("btnDonateWithPaypal")

    $("#btnOptionB").innerHTML += ' <i class="arrow down"></i>'
    $("#btnOptions option[value='donate']").innerHTML += " &#10084;";

    var cStyle = getComputedStyle(document.querySelector("#btnOptionB"))
    btnOptions.style.width = (parseInt(cStyle.width) + 0) + "px"

    // Avoid outputting the error message "Receiving end does not exist" in the Console.
    function checkedLastError() {
        chrome.runtime.lastError
    }

    // fill language list
    {
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
            if (value[0] === "zh" || value[0] === "un" || value[0] === "und") return;
            const option = document.createElement("option")
            option.value = value[0]
            option.textContent = value[1]
            eAllLangs.appendChild(option)
        })

        const eRecentsLangs = selectTargetLanguage.querySelector('[name="recents"]')
        twpConfig.get("targetLanguages").forEach(value => {
            const option = document.createElement("option")
            option.value = value
            option.textContent = langs[value]
            eRecentsLangs.appendChild(option)
        })
    }
    selectTargetLanguage.value = twpConfig.get("targetLanguages")[0]


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
                color: #e8e6e3 !important;
                background-color: #181a1b !important;
                border: 1px solid #454a4d;
            }
            
            #btnSwitchInterfaces:hover {
                background-color: #454a4d !important;
                color: rgb(231, 230, 228) !important;
            }
            
            #selectTargetLanguage, select, option, #btnReset, #btnRestore, #btnTryAgain, #btnOptionB {
                color: #55a9ed !important;
                background-color: #181a1b !important;
                border: 1px solid #454a4d !important;
            }
            `
            document.head.appendChild(el)
        }
    }

    function disableDarkMode() {
        if (document.getElementById("darkModeElement")) {
            document.getElementById("darkModeElement").remove()
        }
    }

    if (twpConfig.get("darkMode") == "auto") {
        if (matchMedia("(prefers-color-scheme: dark)").matches) {
            enableDarkMode()
        } else {
            disableDarkMode()
        }
    } else if (twpConfig.get("darkMode") == "yes") {
        enableDarkMode()
    } else {
        disableDarkMode()
    }

    let originalTabLanguage = "und"
    let currentPageLanguage = "und"
    let currentPageLanguageState = "original"
    let currentPageTranslatorService = twpConfig.get("pageTranslatorService")

    chrome.tabs.query({
        active: true,
        currentWindow: true
    }, tabs => {
        chrome.tabs.sendMessage(tabs[0].id, {
            action: "getOriginalTabLanguage"
        }, {
            frameId: 0
        }, tabLanguage => {
            checkedLastError()
            if (tabLanguage && (tabLanguage = twpLang.checkLanguageCode(tabLanguage))) {
                originalTabLanguage = tabLanguage
            }
        })

        chrome.tabs.sendMessage(tabs[0].id, {
            action: "getCurrentPageLanguage"
        }, {
            frameId: 0
        }, pageLanguage => {
            checkedLastError()
            if (pageLanguage) {
                currentPageLanguage = pageLanguage
                updateInterface()
            }
        })

        chrome.tabs.sendMessage(tabs[0].id, {
            action: "getCurrentPageLanguageState"
        }, {
            frameId: 0
        }, pageLanguageState => {
            checkedLastError()
            if (pageLanguageState) {
                currentPageLanguageState = pageLanguageState
                updateInterface()
            }
        })

        chrome.tabs.sendMessage(tabs[0].id, {
            action: "getCurrentPageTranslatorService"
        }, {
            frameId: 0
        }, pageTranslatorService => {
            checkedLastError()
            if (pageTranslatorService) {
                currentPageTranslatorService = pageTranslatorService
                updateInterface()
            }
        })
    })

    let showSelectTargetLanguage = false

    function updateInterface() {
        if (currentPageTranslatorService == "yandex") {
            $("#btnOptions option[value='translateInExternalSite']").textContent = chrome.i18n.getMessage("msgOpenOnYandexTranslator")
            $("#iconTranslate").setAttribute("src", "/icons/yandex-translate-32.png")
        } else { // google
            $("#btnOptions option[value='translateInExternalSite']").textContent = chrome.i18n.getMessage("btnOpenOnGoogleTranslate")
            $("#iconTranslate").setAttribute("src", "/icons/google-translate-32.png")
        }

        let showAlwaysTranslateCheckbox = false

        if (originalTabLanguage !== "und") {
            $("#cbAlwaysTranslateThisLang").checked = twpConfig.get("alwaysTranslateLangs").indexOf(originalTabLanguage) !== -1
            $("#lblAlwaysTranslateThisLang").textContent = chrome.i18n.getMessage("lblAlwaysTranslate", twpLang.codeToLanguage(originalTabLanguage))
            $("#divAlwaysTranslateThisLang").style.display = "block"

            const translatedWhenHoveringThisLangText = chrome.i18n.getMessage("lblShowTranslatedWhenHoveringThisLang", twpLang.codeToLanguage(originalTabLanguage))
            if (twpConfig.get("langsToTranslateWhenHovering").indexOf(originalTabLanguage) === -1) {
                $("option[data-i18n=lblShowTranslatedWhenHoveringThisLang]").textContent = translatedWhenHoveringThisLangText
            } else {
                $("option[data-i18n=lblShowTranslatedWhenHoveringThisLang]").textContent = "✔ " + translatedWhenHoveringThisLangText
            }
            $("option[data-i18n=lblShowTranslatedWhenHoveringThisLang]").removeAttribute("hidden")

            const neverTranslateLangText = chrome.i18n.getMessage("btnNeverTranslateThisLanguage")
            if (twpConfig.get("neverTranslateLangs").indexOf(originalTabLanguage) === -1) {
                $("option[data-i18n=btnNeverTranslateThisLanguage]").textContent = neverTranslateLangText
            } else {
                $("option[data-i18n=btnNeverTranslateThisLanguage]").textContent = "✔ " + neverTranslateLangText
            }
            $("option[data-i18n=btnNeverTranslateThisLanguage]").style.display = "block"

            showAlwaysTranslateCheckbox = true
        }

        btnRestore.className = btnRestore.className.replace(" w3-disabled", "")

        if (showSelectTargetLanguage) {
            lblTranslate.style.display = "none"
            lblTranslating.style.display = "none"
            lblTranslated.style.display = "none"
            lblError.style.display = "none"
            lblTargetLanguage.style.display = "inline"

            selectTargetLanguage.style.display = "inline"
            btnReset.style.display = "inline"

            divAlwaysTranslate.style.display = "none"
            btnTranslate.style.display = "inline"
            btnRestore.style.display = "none"
            btnTryAgain.style.display = "none"
            btnOptionsDiv.style.display = "none"
        } else {
            divIconTranslateContainer.style.display = "none"
            lblTargetLanguage.style.display = "none"
            selectTargetLanguage.style.display = "none"
            btnReset.style.display = "none"
            switch (currentPageLanguageState) {
                case "translated":
                    lblTranslate.style.display = "none"
                    lblTranslating.style.display = "none"
                    lblTranslated.style.display = "inline"
                    lblError.style.display = "none"

                    divAlwaysTranslate.style.display = "none"
                    btnTranslate.style.display = "none"
                    btnRestore.style.display = "inline"
                    btnTryAgain.style.display = "none"
                    btnOptionsDiv.style.display = "inline"
                    break;
                case "translating":
                    lblTranslate.style.display = "none"
                    lblTranslating.style.display = "inline"
                    lblTranslated.style.display = "none"
                    lblError.style.display = "none"

                    divAlwaysTranslate.style.display = "none"
                    btnTranslate.style.display = "none"
                    btnRestore.style.display = "inline"
                    btnTryAgain.style.display = "none"
                    btnOptionsDiv.style.display = "none"

                    if (btnRestore.className.indexOf("w3-disabled") == -1) {
                        btnRestore.className += " w3-disabled";
                    }
                    break;
                case "error":
                    lblTranslate.style.display = "none"
                    lblTranslating.style.display = "none"
                    lblTranslated.style.display = "none"
                    lblError.style.display = "inline"

                    divAlwaysTranslate.style.display = "none"
                    btnTranslate.style.display = "none"
                    btnRestore.style.display = "none"
                    btnTryAgain.style.display = "inline"
                    btnOptionsDiv.style.display = "none"

                    divIconTranslateContainer.style.display = "block"
                    break;
                default:
                    lblTranslate.style.display = "inline"
                    lblTranslating.style.display = "none"
                    lblTranslated.style.display = "none"
                    lblError.style.display = "none"

                    showAlwaysTranslateCheckbox ? divAlwaysTranslate.style.display = "block" : divAlwaysTranslate.style.display = "none";
                    btnTranslate.style.display = "inline"
                    btnRestore.style.display = "none"
                    btnTryAgain.style.display = "none"
                    btnOptionsDiv.style.display = "inline"

                    divIconTranslateContainer.style.display = "block"
                    break;
            }
        }
    }
    updateInterface()

    $("#btnTranslate").onclick = e => {
        currentPageLanguageState = "translated"

        chrome.tabs.query({
            active: true,
            currentWindow: true
        }, tabs => {
            if (twpConfig.get("targetLanguage") !== selectTargetLanguage.value) {
                twpConfig.setTargetLanguage(selectTargetLanguage.value, true)
            } else {
                twpConfig.setTargetLanguage(selectTargetLanguage.value)
            }
            chrome.tabs.sendMessage(tabs[0].id, {
                action: "translatePage",
                targetLanguage: selectTargetLanguage.value
            }, checkedLastError)
        })

        showSelectTargetLanguage = false
        updateInterface()
    }

    $("#btnRestore").onclick = e => {
        currentPageLanguageState = "original"

        chrome.tabs.query({
            active: true,
            currentWindow: true
        }, tabs => {
            chrome.tabs.sendMessage(tabs[0].id, {
                action: "restorePage"
            }, checkedLastError)
        })

        updateInterface()
    }


    $("#divIconTranslate").addEventListener("click", () => {
        chrome.tabs.query({
            active: true,
            currentWindow: true
        }, tabs => {
            chrome.tabs.sendMessage(tabs[0].id, {
                action: "swapTranslationService"
            }, checkedLastError)
        })

        if (currentPageTranslatorService === "google") {
            currentPageTranslatorService = "yandex"
        } else {
            currentPageTranslatorService = "google"
        }

        twpConfig.set("pageTranslatorService", currentPageTranslatorService)

        updateInterface()
    })

    chrome.tabs.query({
        active: true,
        currentWindow: true
    }, tabs => {
        $("#cbAlwaysTranslateThisLang").addEventListener("change", e => {
            const hostname = new URL(tabs[0].url).hostname
            if (e.target.checked) {
                twpConfig.addLangToAlwaysTranslate(originalTabLanguage, hostname)
            } else {
                twpConfig.removeLangFromAlwaysTranslate(originalTabLanguage)
            }
        })
    })

    $("#btnOptions").addEventListener("change", event => {
        const btnOptions = event.target

        chrome.tabs.query({
            active: true,
            currentWindow: true
        }, tabs => {
            const hostname = new URL(tabs[0].url).hostname
            switch (btnOptions.value) {
                case "changeLanguage":
                    showSelectTargetLanguage = true
                    updateInterface()
                    break
                case "alwaysTranslateThisSite":
                    if (twpConfig.get("alwaysTranslateSites").indexOf(hostname) === -1) {
                        twpConfig.addSiteToAlwaysTranslate(hostname)
                    } else {
                        twpConfig.removeSiteFromAlwaysTranslate(hostname)
                    }
                    window.close()
                    break
                case "neverTranslateThisSite":
                    if (twpConfig.get("neverTranslateSites").indexOf(hostname) === -1) {
                        twpConfig.addSiteToNeverTranslate(hostname)
                    } else {
                        twpConfig.removeSiteFromNeverTranslate(hostname)
                    }
                    window.close()
                    break
                case "alwaysTranslateThisLanguage":
                    twpConfig.addLangToAlwaysTranslate(originalTabLanguage, hostname)
                    break
                case "neverTranslateThisLanguage":
                    if (twpConfig.get("neverTranslateLangs").indexOf(originalTabLanguage) === -1) {
                        twpConfig.addLangToNeverTranslate(originalTabLanguage, hostname)
                    } else {
                        twpConfig.removeLangFromNeverTranslate(originalTabLanguage)
                    }
                    window.close()
                    break
                case "showTranslateSelectedButton":
                    if (twpConfig.get("showTranslateSelectedButton") === "yes") {
                        twpConfig.set("showTranslateSelectedButton", "no")
                    } else {
                        twpConfig.set("showTranslateSelectedButton", "yes")
                    }
                    window.close()
                    break
                case "showOriginalTextWhenHovering":
                    if (twpConfig.get("showOriginalTextWhenHovering") === "yes") {
                        twpConfig.set("showOriginalTextWhenHovering", "no")
                    } else {
                        twpConfig.set("showOriginalTextWhenHovering", "yes")
                    }
                    window.close()
                    break
                case "showTranslatedWhenHoveringThisSite":
                    if (twpConfig.get("sitesToTranslateWhenHovering").indexOf(hostname) === -1) {
                        twpConfig.addSiteToTranslateWhenHovering(hostname)
                    } else {
                        twpConfig.removeSiteFromTranslateWhenHovering(hostname)
                    }
                    window.close()
                    break
                case "showTranslatedWhenHoveringThisLang":
                    if (twpConfig.get("langsToTranslateWhenHovering").indexOf(originalTabLanguage) === -1) {
                        twpConfig.addLangToTranslateWhenHovering(originalTabLanguage)
                    } else {
                        twpConfig.removeLangFromTranslateWhenHovering(originalTabLanguage)
                    }
                    window.close()
                    break
                case "translateInExternalSite":
                    chrome.tabs.query({
                        active: true,
                        currentWindow: true
                    }, tabs => {
                        if (currentPageTranslatorService === "yandex") {
                            chrome.tabs.create({
                                url: "https://translate.yandex.com/translate?url=" + encodeURIComponent(tabs[0].url)
                            })
                        } else { // google
                            chrome.tabs.create({
                                url: `https://translate.google.${
                                "zh-cn" == navigator.language.toLowerCase() ? "cn" : "com"
                            }/translate?u=` + encodeURIComponent(tabs[0].url)
                            })
                        }
                    })
                    break
                case "moreOptions":
                    chrome.tabs.create({
                        url: chrome.runtime.getURL("/options/options.html")
                    })
                    break
                case "donate":
                    chrome.tabs.create({
                        url: chrome.runtime.getURL("/options/options.html#donation")
                    })
                    break
                default:
                    break
            }
            btnOptions.value = "options"
        })
    })

    chrome.tabs.query({
        active: true,
        currentWindow: true
    }, tabs => {
        const hostname = new URL(tabs[0].url).hostname

        const btnNeverTranslateText = chrome.i18n.getMessage("btnNeverTranslate")
        if (twpConfig.get("neverTranslateSites").indexOf(hostname) === -1) {
            $("option[data-i18n=btnNeverTranslate]").textContent = btnNeverTranslateText
        } else {
            $("option[data-i18n=btnNeverTranslate]").textContent = "✔ " + btnNeverTranslateText
        }

        const btnAlwaysTranslateText = chrome.i18n.getMessage("btnAlwaysTranslate")
        if (twpConfig.get("alwaysTranslateSites").indexOf(hostname) === -1) {
            $("option[data-i18n=btnAlwaysTranslate]").textContent = btnAlwaysTranslateText
        } else {
            $("option[data-i18n=btnAlwaysTranslate]").textContent = "✔ " + btnAlwaysTranslateText
        }

        {
            const text = chrome.i18n.getMessage("lblShowTranslateSelectedButton")
            if (twpConfig.get("showTranslateSelectedButton") !== "yes") {
                $("option[data-i18n=lblShowTranslateSelectedButton]").textContent = text
            } else {
                $("option[data-i18n=lblShowTranslateSelectedButton]").textContent = "✔ " + text
            }
        } {
            const text = chrome.i18n.getMessage("lblShowOriginalTextWhenHovering")
            if (twpConfig.get("showOriginalTextWhenHovering") !== "yes") {
                $("option[data-i18n=lblShowOriginalTextWhenHovering]").textContent = text
            } else {
                $("option[data-i18n=lblShowOriginalTextWhenHovering]").textContent = "✔ " + text
            }
        } {
            const text = chrome.i18n.getMessage("lblShowTranslatedWhenHoveringThisSite")
            if (twpConfig.get("sitesToTranslateWhenHovering").indexOf(hostname) === -1) {
                $("option[data-i18n=lblShowTranslatedWhenHoveringThisSite]").textContent = text
            } else {
                $("option[data-i18n=lblShowTranslatedWhenHoveringThisSite]").textContent = "✔ " + text
            }
        }
    })
})