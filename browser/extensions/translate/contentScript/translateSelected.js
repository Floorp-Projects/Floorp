"use strict";

//TODO desativar o botão caso a pagina esteja traduzida

var translateSelected = {}

function getTabHostName() {
    return new Promise(resolve => chrome.runtime.sendMessage({action: "getTabHostName"}, result => resolve(result)))
}

Promise.all([twpConfig.onReady(), getTabHostName()])
.then(function (_) {
    const tabHostName = _[1]
    let styleTextContent = ""
    fetch(chrome.runtime.getURL("/contentScript/css/translateSelected.css"))
        .then(response => response.text())
        .then(response => {
            styleTextContent = response.replace('/icons/icon-32.png', chrome.runtime.getURL("/icons/icon-32.png"))
        })
        .catch(e => console.error(e))

    let divElement
    let eButtonTransSelText
    let eDivResult
    let eSelTextTrans
    let eOrigText
    let eOrigTextDiv

    let originalTabLanguage = "und"
    let currentTargetLanguages = twpConfig.get("targetLanguages")
    let currentTargetLanguage = twpConfig.get("targetLanguageTextTranslation")
    let currentTextTranslatorService = twpConfig.get("textTranslatorService")
    let awaysTranslateThisSite = twpConfig.get("alwaysTranslateSites").indexOf(tabHostName) !== -1
    let translateThisSite = twpConfig.get("neverTranslateSites").indexOf(tabHostName) === -1
    let translateThisLanguage = twpConfig.get("neverTranslateLangs").indexOf(originalTabLanguage) === -1
    let showTranslateSelectedButton = twpConfig.get("showTranslateSelectedButton")
    let dontShowIfPageLangIsTargetLang = twpConfig.get("dontShowIfPageLangIsTargetLang")
    let dontShowIfPageLangIsUnknown = twpConfig.get("dontShowIfPageLangIsUnknown")
    let dontShowIfSelectedTextIsTargetLang = twpConfig.get("dontShowIfSelectedTextIsTargetLang")
    let dontShowIfSelectedTextIsUnknown = twpConfig.get("dontShowIfSelectedTextIsUnknown")
    let fooCount = 0

    pageTranslator.onGetOriginalTabLanguage(function (tabLanguage) {
        originalTabLanguage = tabLanguage
        translateThisLanguage = twpConfig.get("neverTranslateLangs").indexOf(originalTabLanguage) === -1
        updateEventListener()
    })

    async function detectTextLanguage(text) {
        if (!chrome.i18n.detectLanguage) return "und"

        return await new Promise(resolve => {
            chrome.i18n.detectLanguage(text, result => {
                if (!result) return resolve("und")

                for (const langInfo of result.languages) {
                    const langCode = twpLang.checkLanguageCode(langInfo.language)
                    if (langCode) {
                        return resolve(langCode)
                    }
                }

                return resolve("und")
            })
        })
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

    function dragElement(elmnt, elmnt2) {
        var pos1 = 0,
            pos2 = 0,
            pos3 = 0,
            pos4 = 0;
        if (elmnt2) {
            elmnt2.addEventListener("mousedown", dragMouseDown);
        } else {
            elmnt.addEventListener("mousedown", dragMouseDown);
        }

        function dragMouseDown(e) {
            e = e || window.event;
            e.preventDefault();
            // get the mouse cursor position at startup:
            pos3 = e.clientX;
            pos4 = e.clientY;
            document.addEventListener("mouseup", closeDragElement);
            // call a function whenever the cursor moves:
            document.addEventListener("mousemove", elementDrag);
        }

        function elementDrag(e) {
            e = e || window.event;
            e.preventDefault();
            // calculate the new cursor position:
            pos1 = pos3 - e.clientX;
            pos2 = pos4 - e.clientY;
            pos3 = e.clientX;
            pos4 = e.clientY;
            // set the element's new position:
            elmnt.style.top = (elmnt.offsetTop - pos2) + "px";
            elmnt.style.left = (elmnt.offsetLeft - pos1) + "px";
        }

        function closeDragElement() {
            // stop moving when mouse button is released:
            document.removeEventListener("mouseup", closeDragElement);
            document.removeEventListener("mousemove", elementDrag);
        }
    }

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

    function init() {
        destroy()

        window.isTranslatingSelected = true

        divElement = document.createElement("div")
        divElement.style = "all: initial"
        divElement.classList.add("notranslate")

        const shadowRoot = divElement.attachShadow({
            mode: "closed"
        })
        shadowRoot.innerHTML = `
        <link rel="stylesheet" href="${chrome.runtime.getURL("/contentScript/css/translateSelected.css")}">

        <div id="eButtonTransSelText"></div>
            <div id="eDivResult">
                <div id="eOrigTextDiv">
                    <div id="eOrigText" contentEditable="true" spellcheck="false" dir="auto"></div>
                    <hr>
                </div>
                <div id="eSelTextTrans" dir="auto"></div>
                <hr>
                <div id="drag">
                    <ul id="setTargetLanguage">
                        <li value="en" title="English">en</li>
                        <li value="es" title="Spanish">es</li>
                        <li value="de" title="German">de</li>
                    </ul>
                    <div id="moreOrLess"><i class="arrow up" id="more"></i><i class="arrow down" id="less"></i></div>
                    <ul>
                        <li title="Google" id="sGoogle">g</li>
                        <li title="Yandex" id="sYandex">y</li>
                        <li title="Bing" id="sBing">b</li>
                        <li title="DeepL" id="sDeepL" hidden>d</li>
                        <li title="Listen" data-i18n-title="btnListen" id="listen">
                            <svg version="1.1" id="Capa_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
                            width="14px" height="12px" viewBox="0 0 93.038 93.038" xml:space="preserve">
                            <g>
                                <path d="M46.547,75.521c0,1.639-0.947,3.128-2.429,3.823c-0.573,0.271-1.187,0.402-1.797,0.402c-0.966,0-1.923-0.332-2.696-0.973
                                    l-23.098-19.14H4.225C1.892,59.635,0,57.742,0,55.409V38.576c0-2.334,1.892-4.226,4.225-4.226h12.303l23.098-19.14
                                    c1.262-1.046,3.012-1.269,4.493-0.569c1.481,0.695,2.429,2.185,2.429,3.823L46.547,75.521L46.547,75.521z M62.784,68.919
                                    c-0.103,0.007-0.202,0.011-0.304,0.011c-1.116,0-2.192-0.441-2.987-1.237l-0.565-0.567c-1.482-1.479-1.656-3.822-0.408-5.504
                                    c3.164-4.266,4.834-9.323,4.834-14.628c0-5.706-1.896-11.058-5.484-15.478c-1.366-1.68-1.24-4.12,0.291-5.65l0.564-0.565
                                    c0.844-0.844,1.975-1.304,3.199-1.231c1.192,0.06,2.305,0.621,3.061,1.545c4.977,6.09,7.606,13.484,7.606,21.38
                                    c0,7.354-2.325,14.354-6.725,20.24C65.131,68.216,64.007,68.832,62.784,68.919z M80.252,81.976
                                    c-0.764,0.903-1.869,1.445-3.052,1.495c-0.058,0.002-0.117,0.004-0.177,0.004c-1.119,0-2.193-0.442-2.988-1.237l-0.555-0.555
                                    c-1.551-1.55-1.656-4.029-0.246-5.707c6.814-8.104,10.568-18.396,10.568-28.982c0-11.011-4.019-21.611-11.314-29.847
                                    c-1.479-1.672-1.404-4.203,0.17-5.783l0.554-0.555c0.822-0.826,1.89-1.281,3.115-1.242c1.163,0.033,2.263,0.547,3.036,1.417
                                    c8.818,9.928,13.675,22.718,13.675,36.01C93.04,59.783,88.499,72.207,80.252,81.976z"/>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            <g>
                            </g>
                            </svg>
                        </li>
                    </ul>
                </div>
            </div>
        `

        {
            const style = document.createElement("style")
            style.textContent = styleTextContent
            shadowRoot.insertBefore(style, shadowRoot.getElementById("eButtonTransSelText"))
        }

        dragElement(shadowRoot.getElementById("eDivResult"), shadowRoot.getElementById("drag"))

        function enableDarkMode() {
            if (!shadowRoot.getElementById("darkModeElement")) {
                const el = document.createElement("style")
                el.setAttribute("id", "darkModeElement")
                el.setAttribute("rel", "stylesheet")
                el.textContent = `
                    * {
                        scrollbar-color: #202324 #454a4d;
                    }
                    #eDivResult {
                        color: rgb(231, 230, 228) !important;
                        background-color: #181a1b !important;
                    }
                    hr {
                        border-color: #666;
                    }
                    li:hover, #moreOrLess:hover {
                        color: rgb(231, 230, 228) !important;
                        background-color: #454a4d !important;
                    }
                    .selected {
                        background-color: #454a4d !important;
                    }
                    .arrow {
                        border: solid rgb(231, 230, 228);
                        border-width: 0 1px 1px 0;
                    }
                `
                shadowRoot.appendChild(el)
                shadowRoot.querySelector("#listen svg").style = "fill: rgb(231, 230, 228)"
            }
        }

        function disableDarkMode() {
            if (shadowRoot.getElementById("#darkModeElement")) {
                shadowRoot.getElementById("#darkModeElement").remove()
                shadowRoot.querySelector("#listen svg").style = "fill: black"
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

        eButtonTransSelText = shadowRoot.getElementById("eButtonTransSelText")
        eDivResult = shadowRoot.getElementById("eDivResult")
        eSelTextTrans = shadowRoot.getElementById("eSelTextTrans")
        eOrigText = shadowRoot.getElementById("eOrigText")
        eOrigTextDiv = shadowRoot.getElementById("eOrigTextDiv")

        const eMoreOrLess = shadowRoot.getElementById("moreOrLess")
        const eMore = shadowRoot.getElementById("more")
        const eLess = shadowRoot.getElementById("less")

        const sGoogle = shadowRoot.getElementById("sGoogle")
        const sYandex = shadowRoot.getElementById("sYandex")
        const sBing = shadowRoot.getElementById("sBing")
        const sDeepL = shadowRoot.getElementById("sDeepL")

        eOrigText.onkeypress = e => {
            e.stopPropagation()
        }

        eOrigText.onkeydown = e => {
            e.stopPropagation()
        }

        let translateNewInputTimerHandler
        eOrigText.oninput = () => {
            clearTimeout(translateNewInputTimerHandler)
            translateNewInputTimerHandler = setTimeout(translateNewInput, 800)
        }

        eMoreOrLess.onclick = () => {
            if (twpConfig.get("expandPanelTranslateSelectedText") === "no") {
                twpConfig.set("expandPanelTranslateSelectedText", "yes")
            } else {
                twpConfig.set("expandPanelTranslateSelectedText", "no")
            }

            setCaretAtEnd()
        }

        sGoogle.onclick = () => {
            currentTextTranslatorService = "google"
            twpConfig.set("textTranslatorService", "google")
            translateNewInput()

            sGoogle.classList.remove("selected")
            sYandex.classList.remove("selected")
            sBing.classList.remove("selected")
            sDeepL.classList.remove("selected")

            sGoogle.classList.add("selected")
        }
        sYandex.onclick = () => {
            currentTextTranslatorService = "yandex"
            twpConfig.set("textTranslatorService", "yandex")
            translateNewInput()

            sGoogle.classList.remove("selected")
            sYandex.classList.remove("selected")
            sBing.classList.remove("selected")
            sDeepL.classList.remove("selected")

            sYandex.classList.add("selected")
        }
        sBing.onclick = () => {
            currentTextTranslatorService = "bing"
            twpConfig.set("textTranslatorService", "bing")
            translateNewInput()

            sGoogle.classList.remove("selected")
            sYandex.classList.remove("selected")
            sBing.classList.remove("selected")
            sDeepL.classList.remove("selected")

            sBing.classList.add("selected")
        }
        sDeepL.onclick = () => {
            currentTextTranslatorService = "deepl"
            twpConfig.set("textTranslatorService", "deepl")
            translateNewInput()

            sGoogle.classList.remove("selected")
            sYandex.classList.remove("selected")
            sBing.classList.remove("selected")
            sDeepL.classList.remove("selected")

            sDeepL.classList.add("selected")
        }

        const setTargetLanguage = shadowRoot.getElementById("setTargetLanguage")
        setTargetLanguage.onclick = e => {
            if (e.target.getAttribute("value")) {
                const langCode = twpLang.checkLanguageCode(e.target.getAttribute("value"))
                if (langCode) {
                    currentTargetLanguage = langCode
                    twpConfig.setTargetLanguageTextTranslation(langCode)
                    translateNewInput()
                }

                shadowRoot.querySelectorAll("#setTargetLanguage li").forEach(li => {
                    li.classList.remove("selected")
                })

                e.target.classList.add("selected")
            }
        }

        const eListen = shadowRoot.getElementById("listen")
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
                    text: eSelTextTrans.textContent,
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

        document.body.appendChild(divElement)

        chrome.i18n.translateDocument(shadowRoot)

        if (platformInfo.isMobile.any) {
            eButtonTransSelText.style.width = "30px"
            eButtonTransSelText.style.height = "30px"
            document.addEventListener("touchstart", onTouchstart)
        }

        eButtonTransSelText.addEventListener("click", onClick)
        document.addEventListener("mousedown", onDown)

        const targetLanguageButtons = shadowRoot.querySelectorAll("#setTargetLanguage li")

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
        if (twpConfig.get("expandPanelTranslateSelectedText") === "yes") {
            eOrigTextDiv.style.display = "block"
            eMore.style.display = "none"
            eLess.style.display = "block"
            eMoreOrLess.setAttribute("title", chrome.i18n.getMessage("less"))
        } else {
            eOrigTextDiv.style.display = "none"
            eMore.style.display = "block"
            eLess.style.display = "none"
            eMoreOrLess.setAttribute("title", chrome.i18n.getMessage("more"))
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
                case "expandPanelTranslateSelectedText":
                    if (newvalue === "yes") {
                        eOrigTextDiv.style.display = "block"
                        eMore.style.display = "none"
                        eLess.style.display = "block"
                        eMoreOrLess.setAttribute("title", chrome.i18n.getMessage("less"))
                    } else {
                        eOrigTextDiv.style.display = "none"
                        eMore.style.display = "block"
                        eLess.style.display = "none"
                        eMoreOrLess.setAttribute("title", chrome.i18n.getMessage("more"))
                    }
                    break
            }
        })
    }

    function destroy() {
        window.isTranslatingSelected = false
        fooCount++
        stopAudio()
        audioDataUrls = null
        if (!divElement) return;

        eButtonTransSelText.removeEventListener("click", onClick)
        document.removeEventListener("mousedown", onDown)
        if (platformInfo.isMobile.any) {
            document.removeEventListener("touchstart", onTouchstart)
        }
        divElement.remove()
        divElement = eButtonTransSelText = eDivResult = null
    }

    function destroyIfButtonIsShowing(e) {
        if (eButtonTransSelText && e.target !== divElement && eButtonTransSelText.style.display === "block") {
            destroy()
        }
    }

    twpConfig.onChanged(function (name, newValue) {
        switch (name) {
            case "textTranslatorService":
                currentTextTranslatorService = newValue
                break
            case "targetLanguages":
                currentTargetLanguages = newValue
                break
            case "targetLanguageTextTranslation":
                currentTargetLanguage = newValue
                break
            case "alwaysTranslateSites":
                awaysTranslateThisSite = newValue.indexOf(tabHostName) !== -1
                updateEventListener()
                break
            case "neverTranslateSites":
                translateThisSite = newValue.indexOf(tabHostName) === -1
                updateEventListener()
                break
            case "neverTranslateLangs":
                translateThisLanguage = newValue.indexOf(originalTabLanguage) === -1
                updateEventListener()
                break
            case "showTranslateSelectedButton":
                showTranslateSelectedButton = newValue
                updateEventListener()
                break
            case "dontShowIfPageLangIsTargetLang":
                dontShowIfPageLangIsTargetLang = newValue
                updateEventListener()
                break
            case "dontShowIfPageLangIsUnknown":
                dontShowIfPageLangIsUnknown = newValue
                updateEventListener()
                break
            case "dontShowIfSelectedTextIsTargetLang":
                dontShowIfSelectedTextIsTargetLang = newValue
                break
            case "dontShowIfSelectedTextIsUnknown":
                dontShowIfSelectedTextIsUnknown = newValue
                break
        }
    })

    function update_eDivResult(result = "") {
        if (eDivResult.style.display !== "block") {
            init()
        }
        const eTop = prevSelectionInfo.bottom
        const eLeft = prevSelectionInfo.left

        if (twpLang.isRtlLanguage(currentTargetLanguage)) {
            eSelTextTrans.setAttribute("dir", "rtl")
        } else {
            eSelTextTrans.setAttribute("dir", "ltr")
        }
        eSelTextTrans.textContent = result
        if (eDivResult.style.display !== "block") {
            eDivResult.style.display = "block"
            eDivResult.style.top = "0px"
            eDivResult.style.left = "0px"
            eOrigText.textContent = prevSelectionInfo.text

            setCaretAtEnd()

            const height = parseInt(eDivResult.offsetHeight)
            let top = eTop + 5
            top = Math.max(0, top)
            top = Math.min(window.innerHeight - height, top)

            const width = parseInt(eDivResult.offsetWidth)
            let left = parseInt(eLeft /*- width / 2*/ )
            left = Math.max(0, left)
            left = Math.min(window.innerWidth - width, left)

            eDivResult.style.top = top + "px"
            eDivResult.style.left = left + "px"
        }
    }

    function translateNewInput() {
        fooCount++
        let currentFooCount = fooCount
        stopAudio()
        audioDataUrls = null

        backgroundTranslateSingleText(currentTextTranslatorService, currentTargetLanguage, eOrigText.textContent)
            .then(result => {
                if (currentFooCount !== fooCount) return;

                update_eDivResult(result)
            })
    }

    let gSelectionInfo
    let prevSelectionInfo

    function translateSelText(usePrevSelectionInfo = false) {
        if (!usePrevSelectionInfo && gSelectionInfo) {
            prevSelectionInfo = gSelectionInfo
        } else if (!(usePrevSelectionInfo && prevSelectionInfo)) {
            return
        }

        eOrigText.textContent = prevSelectionInfo.text

        translateNewInput()
        const currentFooCount = fooCount
        setTimeout(() => {
            if (currentFooCount !== fooCount) return;
            update_eDivResult(eSelTextTrans.textContent)
            fooCount = currentFooCount
        }, 1000)
    }

    function onClick(e) {
        translateSelText()
        eButtonTransSelText.style.display = "none"
    }

    function onDown(e) {
        if (e.target != divElement) {
            eDivResult.style.display = "none"
            eButtonTransSelText.style.display = "none"
            destroy()
        }
    }


    let isTouchSelection = false

    function onTouchstart(e) {
        isTouchSelection = true
        onDown(e)
    }

    function getSelectionText() {
        let text = "";
        const activeEl = document.activeElement;
        const activeElTagName = activeEl ? activeEl.tagName.toLowerCase() : null;
        if (
            (activeElTagName == "textarea") || (activeElTagName == "input" &&
                /^(?:text|search)$/i.test(activeEl.type)) &&
            (typeof activeEl.selectionStart == "number")
        ) {
            text = activeEl.value.slice(activeEl.selectionStart, activeEl.selectionEnd);
        } else if (window.getSelection) {
            text = window.getSelection().toString();
        }
        return text;
    }

    function readSelection(dontReadIfSelectionDontChange = false) {
        let newSelectionInfo = null

        const activeEl = document.activeElement;
        const activeElTagName = activeEl ? activeEl.tagName.toLowerCase() : null;
        if (
            (activeElTagName == "textarea") || (activeElTagName == "input" &&
                /^(?:text|search)$/i.test(activeEl.type)) &&
            (typeof activeEl.selectionStart == "number")
        ) {
            const text = activeEl.value.slice(activeEl.selectionStart, activeEl.selectionEnd);
            const rect = activeEl.getBoundingClientRect()
            newSelectionInfo = {
                text: text,
                top: rect.top,
                left: rect.left,
                bottom: rect.bottom,
                right: rect.right
            }
        } else if (window.getSelection) {
            const selection = window.getSelection()
            if (selection.type == "Range") {
                const text = selection.toString();
                const rect = selection.getRangeAt(0).getBoundingClientRect()
                newSelectionInfo = {
                    text: text,
                    top: rect.top,
                    left: rect.left,
                    bottom: rect.bottom,
                    right: rect.right
                }
            }
        }

        if (dontReadIfSelectionDontChange && gSelectionInfo && newSelectionInfo && gSelectionInfo.text === newSelectionInfo.text) {
            gSelectionInfo = newSelectionInfo
            return false
        }
        gSelectionInfo = newSelectionInfo
        return true
    }

    async function onUp(e) {
        if (e.target == divElement) return;

        const clientX = Math.max((typeof e.clientX === 'undefined' ? 0 : e.clientX), (typeof e.changedTouches === 'undefined' ? 0 : e.changedTouches[0].clientX));
        const clientY = Math.max((typeof e.clientY === 'undefined' ? 0 : e.clientY), (typeof e.changedTouches === 'undefined' ? 0 : e.changedTouches[0].clientY));

        const selectedText = getSelectionText().trim()
        if (!selectedText || selectedText.length < 1) return;
        let detectedLanguage = await detectTextLanguage(selectedText)
        if (!detectedLanguage) detectedLanguage = "und";

        if (((dontShowIfSelectedTextIsTargetLang == "yes" && detectedLanguage !== currentTargetLanguage) || dontShowIfSelectedTextIsTargetLang != "yes") &&
            ((dontShowIfSelectedTextIsUnknown == "yes" && detectedLanguage !== "und") || dontShowIfSelectedTextIsUnknown != "yes")
        ) {
            init()
            if (platformInfo.isMobile.any) {
                eButtonTransSelText.style.left = window.innerWidth - 45 + "px"
                eButtonTransSelText.style.top = clientY + "px"
            } else {
                eButtonTransSelText.style.left = clientX + 20 + "px"
                eButtonTransSelText.style.top = clientY - 30 + "px"
            }

            eButtonTransSelText.style.display = "block"
        }
    }

    let showButtonTimerHandler = null

    function onMouseup(e) {
        if (e.button != 0) return;
        if (e.target == divElement) return;
        if (readSelection(true)) {
            clearTimeout(showButtonTimerHandler)
            showButtonTimerHandler = setTimeout(() => onUp(e), 150)
        }
    }

    function onTouchend(e) {
        if (e.target == divElement) return;
        readSelection()
        clearTimeout(showButtonTimerHandler)
        showButtonTimerHandler = setTimeout(() => onUp(e), 150)
    }

    function onSelectionchange(e) {
        if (isTouchSelection) {
            readSelection()
        }
    }

    function isSelectingText() {
        const activeEl = document.activeElement;
        const activeElTagName = activeEl ? activeEl.tagName.toLowerCase() : null;
        if (
            (activeElTagName == "textarea") || (activeElTagName == "input" &&
                /^(?:text|search)$/i.test(activeEl.type)) &&
            (typeof activeEl.selectionStart == "number")
        ) {
            const text = activeEl.value.slice(activeEl.selectionStart, activeEl.selectionEnd);
            if (text) return true;
        } else if (window.getSelection) {
            const selection = window.getSelection()
            if (selection.type == "Range") {
                const text = selection.toString();
                if (text) return true;
            }
        }
        return false
    }

    let lastTimePressedCtrl = null

    function onKeyUp(e) {
        if (twpConfig.get("translateSelectedWhenPressTwice") !== "yes") return;
        if (e.key == "Control") {
            if (lastTimePressedCtrl && performance.now() - lastTimePressedCtrl < 280 && isSelectingText()) {
                lastTimePressedCtrl = performance.now()
                readSelection()
                init()
                translateSelText()
            }
            lastTimePressedCtrl = performance.now()
        }
    }
    document.addEventListener("keyup", onKeyUp)

    let windowIsInFocus = true
    window.addEventListener("focus", function (e) {
        windowIsInFocus = true
        chrome.runtime.sendMessage({action: "thisFrameIsInFocus"})
    })
    window.addEventListener("blur", function (e) {
        windowIsInFocus = false
    })

    window.addEventListener("beforeunload", function (e) {
        destroy()
    })

    function updateEventListener() {
        if (showTranslateSelectedButton == "yes" && (awaysTranslateThisSite || (translateThisSite && translateThisLanguage)) &&
            ((dontShowIfPageLangIsTargetLang == "yes" && originalTabLanguage !== currentTargetLanguage) || dontShowIfPageLangIsTargetLang != "yes") &&
            ((dontShowIfPageLangIsUnknown == "yes" && originalTabLanguage !== "und") || dontShowIfPageLangIsUnknown != "yes")
        ) {
            document.addEventListener("mouseup", onMouseup)

            document.addEventListener("blur", destroyIfButtonIsShowing)
            document.addEventListener("visibilitychange", destroyIfButtonIsShowing)

            document.addEventListener("keydown", destroyIfButtonIsShowing)
            document.addEventListener("mousedown", destroyIfButtonIsShowing)
            document.addEventListener("wheel", destroyIfButtonIsShowing)

            if (platformInfo.isMobile.any) {
                document.addEventListener("touchend", onTouchend)
                document.addEventListener("selectionchange", onSelectionchange)
            }
        } else {
            document.removeEventListener("mouseup", onMouseup)

            document.removeEventListener("blur", destroyIfButtonIsShowing)
            document.removeEventListener("visibilitychange", destroyIfButtonIsShowing)

            document.removeEventListener("keydown", destroyIfButtonIsShowing)
            document.removeEventListener("mousedown", destroyIfButtonIsShowing)
            document.removeEventListener("wheel", destroyIfButtonIsShowing)

            if (platformInfo.isMobile.any) {
                document.removeEventListener("touchend", onTouchend)
                document.removeEventListener("selectionchange", onSelectionchange)
            }
        }
    }
    updateEventListener()

    chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
        if (request.action === "TranslateSelectedText") {
            readSelection()
            init()
            translateSelText()
        } else if (request.action === "anotherFrameIsInFocus")  {
            if (!windowIsInFocus) {
                destroy()
            }
        }
    })
})
