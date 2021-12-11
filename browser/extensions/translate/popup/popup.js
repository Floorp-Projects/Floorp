"use strict";

var $ = document.querySelector.bind(document)

twpConfig.onReady(function () {
    let popupPanelSection = twpConfig.get("popupPanelSection")

    function updatePopupSection() {
        document.querySelectorAll("[data-popupPanelSection]").forEach(node => {
            const nodePopupPanelSection = parseInt(node.getAttribute("data-popupPanelSection"))
            if (isNaN(nodePopupPanelSection)) return

            if (nodePopupPanelSection > popupPanelSection) {
                node.style.display = "none"
            } else {
                node.style.display = "block"
            }
        })

        document.querySelectorAll("[data-popupPanelSection2]").forEach(node => {
            const nodePopupPanelSection2 = parseInt(node.getAttribute("data-popupPanelSection2"))
            if (isNaN(nodePopupPanelSection2)) return

            if (nodePopupPanelSection2 <= popupPanelSection) {
                node.style.display = "none"
            } else {
                node.style.display = "block"
            }
        })

        $("#more").style.display = "block"
        $("#less").style.display = "block"

        if (popupPanelSection >= 6) {
            $("#more").style.display = "none"
        } else if (popupPanelSection <= 0) {
            $("#less").style.display = "none"
        }
    }
    updatePopupSection()

    // Avoid outputting the error message "Receiving end does not exist" in the Console.
    function checkedLastError() {
        chrome.runtime.lastError
    }

    $("#more").onclick = e => {
        if (popupPanelSection < 6) {
            popupPanelSection++
            updatePopupSection()
        }
        twpConfig.set("popupPanelSection", popupPanelSection)
    }
    $("#less").onclick = e => {
        if (popupPanelSection > 0) {
            popupPanelSection--
            updatePopupSection()
        }
        twpConfig.set("popupPanelSection", popupPanelSection)
    }

    let originalTabLanguage = "und"
    let currentPageLanguage = "und"
    let currentPageLanguageState = "original"
    let currentPageTranslatorService = twpConfig.get("pageTranslatorService")

    const twpButtons = document.querySelectorAll("button")

    twpButtons.forEach(button => {
        button.addEventListener("click", event => {
            twpButtons.forEach(button => {
                button.classList.remove("w3-buttonSelected")
            })
            event.target.classList.add("w3-buttonSelected")

            currentPageLanguage = event.target.value
            if (currentPageLanguage === "original") {
                currentPageLanguageState = "original"
            } else {
                currentPageLanguageState = "translated"
                twpConfig.setTargetLanguage(event.target.value)
            }

            chrome.tabs.query({
                active: true,
                currentWindow: true
            }, tabs => {
                chrome.tabs.sendMessage(tabs[0].id, {
                    action: "translatePage",
                    targetLanguage: event.target.value || "original"
                }, checkedLastError)
            })
        })
    })

    let targetLanguages = twpConfig.get("targetLanguages")
    for (let i = 1; i < 4; i++) {
        const button = twpButtons[i]
        button.value = targetLanguages[i - 1]
        button.textContent = twpLang.codeToLanguage(targetLanguages[i - 1])
    }

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
            if (!tabLanguage || (tabLanguage = twpLang.checkLanguageCode(tabLanguage))) {
                originalTabLanguage = tabLanguage || "und"
                twpButtons[0].childNodes[1].textContent = twpLang.codeToLanguage(originalTabLanguage)
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

    function updateInterface() {
        if (currentPageTranslatorService == "yandex") {
            $("#btnOptions option[value='translateInExternalSite']").textContent = chrome.i18n.getMessage("msgOpenOnYandexTranslator")
            $("#iconTranslate").setAttribute("src", "/icons/yandex-translate-32.png")
        } else { // google
            $("#btnOptions option[value='translateInExternalSite']").textContent = chrome.i18n.getMessage("btnOpenOnGoogleTranslate")
            $("#iconTranslate").setAttribute("src", "/icons/google-translate-32.png")
        }

        twpButtons.forEach(button => {
            button.classList.remove("w3-buttonSelected")
            if ((currentPageLanguageState !== "translated" && button.value === "original") ||
                (currentPageLanguageState === "translated" && button.value === currentPageLanguage)) {
                button.classList.add("w3-buttonSelected")
            }
        })

        if (originalTabLanguage !== "und") {
            $("#cbAlwaysTranslateThisLang").checked = twpConfig.get("alwaysTranslateLangs").indexOf(originalTabLanguage) !== -1
            $("#lblAlwaysTranslateThisLang").textContent = chrome.i18n.getMessage("lblAlwaysTranslate", twpLang.codeToLanguage(originalTabLanguage))
            $("#divAlwaysTranslateThisLang").style.display = "block"

            const translatedWhenHoveringThisLangText = chrome.i18n.getMessage("lblShowTranslatedWhenHoveringThisLang", twpLang.codeToLanguage(originalTabLanguage))
            $("#cbShowTranslatedWhenHoveringThisLang").checked = twpConfig.get("langsToTranslateWhenHovering").indexOf(originalTabLanguage) !== -1
            $("#lblShowTranslatedWhenHoveringThisLang").textContent = translatedWhenHoveringThisLangText
            $("#divShowTranslatedWhenHoveringThisLang").style.display = "block"

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
        }
    }
    updateInterface()

    function enableDarkMode() {
        if (!$("#darkModeElement")) {
            const el = document.createElement("style")
            el.setAttribute("id", "darkModeElement")
            el.setAttribute("rel", "stylesheet")
            el.textContent = `
            body {
                color: rgb(231, 230, 228) !important;
                background-color: #181a1b !important;
            }
            
            .mdiv, .md, {
                background-color: rgb(231, 230, 228);
            }

            .menuDot {
                background-image:
                    radial-gradient(rgb(231, 230, 228) 2px, transparent 2px),
                    radial-gradient(rgb(231, 230, 228) 2px, transparent 2px),
                    radial-gradient(rgb(231, 230, 228) 2px, transparent 2px);
            }

            #btnSwitchInterfaces:hover, #divMenu:hover {
                background-color: #454a4d !important;
                color: rgb(231, 230, 228) !important;
            }
            
            select {
                color: rgb(231, 230, 228) !important;
                background-color: #181a1b !important;
            }

            hr {
                border-color: #666;
            }

            .arrow {
                border-color: rgb(231, 230, 228);
            }

            #helpSwapInterface span:before {
                border-bottom: 10px solid #88f;
            }
            `
            document.head.appendChild(el)
        }
    }

    function disableDarkMode() {
        if ($("#darkModeElement")) {
            $("#darkModeElement").remove()
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

    $("#btnSwitchInterfaces").addEventListener("click", () => {
        twpConfig.set("useOldPopup", "yes")

        if ($("#helpSwapInterface").style.display === "block") {
            twpConfig.set("dontShowHelpSwapInterface", "yes")
        }

        window.location = "old-popup.html"
    })

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

        $("#cbAlwaysTranslateThisSite").addEventListener("change", e => {
            const hostname = new URL(tabs[0].url).hostname
            if (e.target.checked) {
                twpConfig.addSiteToAlwaysTranslate(hostname)
            } else {
                twpConfig.removeSiteFromAlwaysTranslate(hostname)
            }
        })

        $("#cbShowTranslateSelectedButton").addEventListener("change", e => {
            if (e.target.checked) {
                twpConfig.set("showTranslateSelectedButton", "yes")
            } else {
                twpConfig.set("showTranslateSelectedButton", "no")
            }
        })

        $("#cbShowOriginalWhenHovering").addEventListener("change", e => {
            if (e.target.checked) {
                twpConfig.set("showOriginalTextWhenHovering", "yes")
            } else {
                twpConfig.set("showOriginalTextWhenHovering", "no")
            }
        })

        $("#cbShowTranslatedWhenHoveringThisSite").addEventListener("change", e => {
            const hostname = new URL(tabs[0].url).hostname
            if (e.target.checked) {
                twpConfig.addSiteToTranslateWhenHovering(hostname)
            } else {
                twpConfig.removeSiteFromTranslateWhenHovering(hostname)
            }
        })

        $("#cbShowTranslatedWhenHoveringThisLang").addEventListener("change", e => {
            if (e.target.checked) {
                twpConfig.addLangToTranslateWhenHovering(originalTabLanguage)
            } else {
                twpConfig.removeLangFromTranslateWhenHovering(originalTabLanguage)
            }
        })

        $("#cbShowTranslateSelectedButton").checked = twpConfig.get("showTranslateSelectedButton") == "yes" ? true : false
        $("#cbShowOriginalWhenHovering").checked = twpConfig.get("showOriginalTextWhenHovering") == "yes" ? true : false

        const hostname = new URL(tabs[0].url).hostname
        $("#cbAlwaysTranslateThisSite").checked = twpConfig.get("alwaysTranslateSites").indexOf(hostname) !== -1
        $("#cbShowTranslatedWhenHoveringThisSite").checked = twpConfig.get("sitesToTranslateWhenHovering").indexOf(hostname) !== -1

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

    $("#btnOptions").addEventListener("change", event => {
        const btnOptions = event.target

        chrome.tabs.query({
            active: true,
            currentWindow: true
        }, tabs => {
            const hostname = new URL(tabs[0].url).hostname
            switch (btnOptions.value) {
                case "changeLanguage":
                    location = chrome.runtime.getURL("/popup/popup-change-language.html")
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
                    if (twpConfig.get("alwaysTranslateLangs").indexOf(originalTabLanguage) === -1) {
                        twpConfig.addLangToAlwaysTranslate(originalTabLanguage, hostname)
                    } else {
                        twpConfig.removeLangFromAlwaysTranslate(originalTabLanguage)
                    }
                    window.close()
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
        const textNever = chrome.i18n.getMessage("btnNeverTranslate")
        if (twpConfig.get("neverTranslateSites").indexOf(hostname) === -1) {
            $("option[data-i18n=btnNeverTranslate]").textContent = textNever
        } else {
            $("option[data-i18n=btnNeverTranslate]").textContent = "✔ " + textNever
        }

        const textAlways = chrome.i18n.getMessage("btnAlwaysTranslate")
        if (twpConfig.get("alwaysTranslateSites").indexOf(hostname) === -1) {
            $("option[data-i18n=btnAlwaysTranslate]").textContent = textAlways
        } else {
            $("option[data-i18n=btnAlwaysTranslate]").textContent = "✔ " + textAlways
        }

        $('option[data-i18n=btnDonate]').innerHTML += " &#10084;"
    })

    $("#btnCloseHelpSwapInterface").onclick = e => {
        $("#helpSwapInterface").style.display = "none"
        twpConfig.set("dontShowHelpSwapInterface", "yes")
        document.body.style.minHeight = null
    }

    const installDateTime = twpConfig.get("installDateTime")
    const dontShowHelpSwapInterface = twpConfig.get("dontShowHelpSwapInterface")
    if (installDateTime && !dontShowHelpSwapInterface) {
        const date = new Date();
        date.setDate(date.getDate() - 1)
        if (date.getTime() > installDateTime) {
            let lastTimeShowingHelpSwapInterface = twpConfig.get("lastTimeShowingHelpSwapInterface")
            let showHelpSwapInterface = false
            if (lastTimeShowingHelpSwapInterface) {
                const date = new Date();
                date.setDate(date.getDate() - 1)
                if (date.getTime() > lastTimeShowingHelpSwapInterface) {
                    showHelpSwapInterface = true
                    lastTimeShowingHelpSwapInterface = Date.now()
                    twpConfig.set("lastTimeShowingHelpSwapInterface", lastTimeShowingHelpSwapInterface)
                }
            } else {
                showHelpSwapInterface = true
                lastTimeShowingHelpSwapInterface = Date.now()
                twpConfig.set("lastTimeShowingHelpSwapInterface", lastTimeShowingHelpSwapInterface)
            }

            if (showHelpSwapInterface) {
                $("#helpSwapInterface").style.display = "block"
                document.body.style.minHeight = "150px"
            }
        }
    }
})