"use strict";

var popupMobile = {}

function getTabHostName() {
    return new Promise(resolve => chrome.runtime.sendMessage({action: "getTabHostName"}, result => resolve(result)))
}

Promise.all([twpConfig.onReady(), getTabHostName()])
.then(function (_) {
    const tabHostName = _[1]
    if (!platformInfo.isMobile.any) return;

    const htmlMobile = `
    <link rel="stylesheet" href="${chrome.runtime.getURL("/contentScript/css/popupMobile.css")}">
    
    <div id='element'>
    <div id='main'>
        <img id="iconTranslate">
        <button id="btnOriginal" class="item button" data-i18n="btnMobileOriginal">Original</button>
        <button id="btnTranslate" class="item button" data-i18n="lblTranslated">Translated</button>
        <button id="spin" class="item button">
            <div class="loader button"></div>
        </button>
        <button id="btnMenu" class="item2">
            <div class="dropup">
                <div id="menu" class="dropup-content">
                    <a id="btnChangeLanguages" data-i18n="btnChangeLanguages">Change languages</a>
                    <a id="btnTranslateSelectedText" data-i18n="msgTranslateSelectedText">Translate selected text</a>
                    <a id="btnNeverTranslate" data-i18n="btnNeverTranslate">Never translate this site</a>
                    <a id="neverTranslateThisLanguage" data-i18n="btnNeverTranslateThisLanguage" display="none">Never translate this language</a>
                    <a id="btnMoreOptions" data-i18n="btnMoreOptions">More options</a>
                    <a id="btnDonate" data-i18n="btnDonate" href="https://www.patreon.com/filipeps" target="_blank" rel="noopener noreferrer">Donate</a>
                </div>
            </div>
            <div class="dropup">
                <div id="menuSelectLanguage" class="dropup-content"></div>
            </div>
            <div class="menuDot"></div>
            <div class="menuDot"></div>
            <div class="menuDot"></div>
        </button>
        <button id="btnClose" class="item">&times;</button>
    </div>
    </div>
    `

    let originalTabLanguage = "und"
    let currentTargetLanguage = twpConfig.get("targetLanguage")
    let currentPageTranslatorService = twpConfig.get("pageTranslatorService")
    let awaysTranslateThisSite = twpConfig.get("alwaysTranslateSites").indexOf(tabHostName) !== -1
    let translateThisSite = twpConfig.get("neverTranslateSites").indexOf(tabHostName) === -1
    let translateThisLanguage = false
    let showPopupMobile = twpConfig.get("showPopupMobile")

    twpConfig.onChanged(function (name, newValue) {
        switch (name) {
            case "alwaysTranslateSites":
                awaysTranslateThisSite = newValue.indexOf(tabHostName) !== -1
                popupMobile.show()
                break
            case "neverTranslateSites":
                translateThisSite = newValue.indexOf(tabHostName) === -1
                popupMobile.show()
                break
            case "neverTranslateLangs":
                translateThisLanguage = originalTabLanguage === "und" || (currentTargetLanguage !== originalTabLanguage && newValue.indexOf(originalTabLanguage) === -1)
                popupMobile.show()
                break
            case "showPopupMobile":
                showPopupMobile = newValue
                popupMobile.show()
                break
        }
    })

    let divElement
    let getElemById

    function hideMenu(e) {
        if (!divElement) return;
        if (e.target === divElement) return;

        getElemById("menuSelectLanguage").style.display = "none"
        getElemById("menu").style.display = "none"
    }

    popupMobile.show = function (forceShow = false) {
        popupMobile.hide()

        if (!forceShow && ((!awaysTranslateThisSite && (!translateThisSite || !translateThisLanguage)) || showPopupMobile !== "yes")) return;

        divElement = document.createElement("div")
        divElement.style = "all: initial"
        divElement.classList.add("notranslate")

        const shadowRoot = divElement.attachShadow({
            mode: "closed"
        })
        shadowRoot.innerHTML = htmlMobile

        document.body.appendChild(divElement)

        chrome.i18n.translateDocument(shadowRoot)

        function enableDarkMode() {
            if (!shadowRoot.getElementById("darkModeElement")) {
                const el = document.createElement("style")
                el.setAttribute("id", "darkModeElement")
                el.setAttribute("rel", "stylesheet")
                el.textContent = `
                div, button, select, option {
                    color: rgb(231, 230, 228);
                    background-color: #181a1b !important;
                }
            
                a {
                    color: rgb(231, 230, 228) !important;
                    background-color: #181a1b;
                }
            
                .dropup-content a:hover {
                    background-color: #454a4d;
                }
            
                .menuDot {
                    background-color: rgb(231, 230, 228) !important;
                }
                `
                shadowRoot.appendChild(el)
            }
        }

        function disableDarkMode() {
            if (shadowRoot.getElementById("#darkModeElement")) {
                shadowRoot.getElementById("#darkModeElement").remove()
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

        getElemById = shadowRoot.getElementById.bind(shadowRoot)

        function translatePage(targetLanguage = currentTargetLanguage) {
            getElemById("menuSelectLanguage").style.display = "none"
            getElemById("menu").style.display = "none"

            if (targetLanguage !== currentTargetLanguage) {
                twpConfig.setTargetLanguage(targetLanguage, true)
            }
            pageTranslator.translatePage(targetLanguage)

            getElemById("btnOriginal").style.color = null
            getElemById("btnTranslate").style.color = "#2196F3"
        }

        // fill language list
        ;
        (function () {
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

            const menuSelectLanguage = getElemById("menuSelectLanguage")
            langsSorted.forEach(value => {
                if (value[0] === "zh" || value[0] === "un" || value[0] === "und") return;
                const a = document.createElement("a")
                a.setAttribute("value", value[0])
                a.textContent = value[1]
                a.addEventListener("click", e => {
                    e.stopPropagation()
                    translatePage(value[0])
                })
                menuSelectLanguage.appendChild(a)
            })
        })()

        function updateIcon() {
            if (!getElemById) return;

            if (currentPageTranslatorService === "yandex") {
                getElemById("iconTranslate").src = chrome.runtime.getURL("/icons/yandex-translate-32.png")
            } else {
                getElemById("iconTranslate").src = chrome.runtime.getURL("/icons/google-translate-32.png")
            }
        }
        updateIcon()

        getElemById("iconTranslate").onclick = e => {
            pageTranslator.swapTranslationService()

            currentPageTranslatorService = currentPageTranslatorService === "google" ? "yandex" : "google"
            updateIcon()

            twpConfig.set("pageTranslatorService", currentPageTranslatorService)
        }

        getElemById("btnOriginal").onclick = e => {
            pageTranslator.restorePage()
            if (!getElemById) return;

            getElemById("btnOriginal").style.color = "#2196F3"
            getElemById("btnTranslate").style.color = null
        }

        getElemById("btnTranslate").onclick = e => {
            translatePage()
        }

        getElemById("btnMenu").onclick = e => {
            if (!getElemById) return;
            getElemById("menu").style.display = "block"
            getElemById("menuSelectLanguage").style.display = "none"
        }

        getElemById("btnClose").onclick = e => {
            popupMobile.hide()
        }

        getElemById("btnChangeLanguages").onclick = e => {
            e.stopPropagation()
            if (!getElemById) return;
            getElemById("menuSelectLanguage").style.display = "block"
            getElemById("menu").style.display = "none"
        }

        getElemById("btnTranslateSelectedText").onclick = e => {
            if (twpConfig.get("showTranslateSelectedButton") === "yes") {
                twpConfig.set("showTranslateSelectedButton", "no")
                getElemById("btnTranslateSelectedText").textContent = chrome.i18n.getMessage("msgTranslateSelectedText")
            } else {
                twpConfig.set("showTranslateSelectedButton", "yes")
                getElemById("btnTranslateSelectedText").textContent = chrome.i18n.getMessage("msgNoTranslateSelectedText")
            }
            getElemById("menu").style.display = "none"
            e.stopPropagation()
        }

        getElemById("btnNeverTranslate").onclick = e => {
            twpConfig.addSiteToNeverTranslate(tabHostName)
            popupMobile.hide()
        }

        getElemById("neverTranslateThisLanguage").onclick = e => {
            twpConfig.addLangToNeverTranslate(originalTabLanguage)
            popupMobile.hide()
        }

        getElemById("btnMoreOptions").onclick = e => {
            chrome.runtime.sendMessage({
                action: "openOptionsPage"
            })
        }

        getElemById("btnDonate").onclick = e => {
            e.preventDefault()
            chrome.runtime.sendMessage({
                action: "openDonationPage"
            })
        }

        document.addEventListener("blur", hideMenu)
        document.addEventListener("click", hideMenu)

        if (twpConfig.get("showTranslateSelectedButton") === "yes") {
            getElemById("btnTranslateSelectedText").textContent = chrome.i18n.getMessage("msgNoTranslateSelectedText")
        } else {
            getElemById("btnTranslateSelectedText").textContent = chrome.i18n.getMessage("msgTranslateSelectedText")
        }

        getElemById("btnDonate").innerHTML += " &#10084;"
    }

    popupMobile.hide = function () {
        if (!divElement) return;

        divElement.remove()
        divElement = getElemById = null

        document.removeEventListener("blur", hideMenu)
        document.removeEventListener("click", hideMenu)
    }

    if (showPopupMobile !== "no") {
        window.addEventListener("touchstart", (e) => {
            if (e.touches.length == 3) {
                popupMobile.show(true)
            }
        })
    }

    chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
        if (request.action === "showPopupMobile") {
            popupMobile.show(true)
        }
    })

    pageTranslator.onGetOriginalTabLanguage(function (tabLanguage) {
        if (!tabLanguage || tabLanguage === "und") {
            const lang = twpLang.checkLanguageCode(document.documentElement.lang)
            if (lang) {
                originalTabLanguage = tabLanguage
            }
        } else {
            originalTabLanguage = tabLanguage
        }
        translateThisLanguage = originalTabLanguage === "und" || (currentTargetLanguage !== originalTabLanguage && twpConfig.get("neverTranslateLangs").indexOf(originalTabLanguage) === -1)
        popupMobile.show()
    })

    /*
    const dpr = 1
    const origInnerWidth = window.innerWidth * window.devicePixelRatio
    console.log(origInnerWidth)
    const el = iframeDiv

    function f() {
        el.style.scale = dpr / window.devicePixelRatio
        el.style.width = ( 100 / (dpr / window.devicePixelRatio) ) + "%"
        el.style.left = "0px"
        el.style.bottom = "0px"

        const eWidth = parseFloat(window.innerWidth)

        const dX = (eWidth * window.devicePixelRatio - eWidth) / 2
        const dY = ((50 * (window.devicePixelRatio)) - 50) / (window.devicePixelRatio * 2)
        console.log({
            scale: dpr / window.devicePixelRatio,
            dpr: dpr,
            devicePixelRatio: devicePixelRatio,
            elWidth: getComputedStyle(el).width,
            dx: dX
        })
        el.style.left = - (origInnerWidth - innerWidth) / 2 + "px"
        el.style.bottom = -dY + "px" 
    }
    let oldwidth = null

    function foo() {
        if (oldwidth) {
            if (oldwidth != window.innerWidth) {
                f()
            }
            oldwidth = window.innerWidth
        } else {
            oldwidth = window.innerWidth
            f()
        }
        requestAnimationFrame(foo)
    }
    requestAnimationFrame(foo)
    //*/
})
