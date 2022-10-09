"use strict";

var showTranslated = {}

function getTabHostName() {
    return new Promise(resolve => chrome.runtime.sendMessage({action: "getTabHostName"}, result => resolve(result)))
}

Promise.all([twpConfig.onReady(), getTabHostName()])
.then(function (_) {
    const tabHostName = _[1]
    if (platformInfo.isMobile.any) return;

    let styleTextContent = ""
    fetch(chrome.runtime.getURL("/contentScript/css/showTranslated.css"))
        .then(response => response.text())
        .then(response => styleTextContent = response)
        .catch(e => console.error(e))

    let pageLanguageState = "original"
    let originalTabLanguage = "und"
    let currentTargetLanguages = twpConfig.get("targetLanguages")
    let currentTargetLanguage = twpConfig.get("targetLanguageTextTranslation")
    let currentTextTranslatorService = twpConfig.get("textTranslatorService") === "deepl" ? "google" : twpConfig.get("textTranslatorService")
    let showTranslatedTextWhenHoveringThisSite = twpConfig.get("sitesToTranslateWhenHovering").indexOf(tabHostName) !== -1
    let showTranslatedTextWhenHoveringThisLang = false
    let translateTextOverMouseWhenPressTwice = twpConfig.get("translateTextOverMouseWhenPressTwice") === "yes"
    let fooCount = 0

    twpConfig.onChanged(function (name, newValue) {
        switch (name) {
            case "textTranslatorService":
                currentTextTranslatorService = newValue === "deepl" ? "google" : newValue
                break
            case "targetLanguages":
                currentTargetLanguages = newValue
                break
            case "targetLanguageTextTranslation":
                currentTargetLanguage = newValue
                break
            case "sitesToTranslateWhenHovering":
                showTranslatedTextWhenHoveringThisSite = newValue.indexOf(tabHostName) !== -1
                updateEventListener()
                break
            case "langsToTranslateWhenHovering":
                showTranslatedTextWhenHoveringThisLang = newValue.indexOf(originalTabLanguage) !== -1
                updateEventListener()
                break
            case "translateTextOverMouseWhenPressTwice":
                translateTextOverMouseWhenPressTwice = twpConfig.get("translateTextOverMouseWhenPressTwice") === "yes"
                updateEventListener()
                break
        }
    })

    const htmlTagsInlineText = ['#text', 'A', 'ABBR', 'ACRONYM', 'B', 'BDO', 'BIG', 'CITE', 'DFN', 'EM', 'I', 'LABEL', 'Q', 'S', 'SMALL', 'SPAN', 'STRONG', 'SUB', 'SUP', 'U', 'TT', 'VAR']
    const htmlTagsInlineIgnore = ['BR', 'CODE', 'KBD', 'WBR'] // and input if type is submit or button, and pre depending on settings
    const htmlTagsNoTranslate = ['TITLE', 'SCRIPT', 'STYLE', 'TEXTAREA', 'svg']

    if (twpConfig.get('translateTag_pre') !== 'yes') {
        htmlTagsInlineIgnore.push('PRE')
    }
    twpConfig.onChanged((name, newvalue) => {
        switch (name) {
            case "translateTag_pre":
                const index = htmlTagsInlineIgnore.indexOf('PRE')
                if (index !== -1) {
                    htmlTagsInlineIgnore.splice(index, 1)
                }
                if (newvalue !== 'yes') {
                    htmlTagsInlineIgnore.push('PRE')
                }
                break
        }
    })

    let divElement
    let shadowRoot
    let eTextTranslated
    let currentNodeOverMouse
    let timeoutHandler

    function onScroll(e) {
        clearTimeout(timeoutHandler)
    }

    const mousePos = {
        x: 0,
        y: 0
    }

    function onMouseMove(e) {
        mousePos.x = e.clientX
        mousePos.y = e.clientY

        if (e.target === divElement) return;

        if (e.target === currentNodeOverMouse) return;
        currentNodeOverMouse = e.target

        if (!(!showTranslatedTextWhenHoveringThisSite && !showTranslatedTextWhenHoveringThisLang)) {
            destroy()
            if (e.buttons === 0) {
                timeoutHandler = setTimeout(translateThisNode, 1250, e.target)
            }
        }
    }

    function onMouseDown(e) {
        if (e.target === divElement) return;
        destroy()
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

    window.addEventListener("beforeunload", e => {
        stopAudio()
        audioDataUrls = null
    })

    let prevNode = null

    function translateThisNode(node, usePrevNode = false) {
        fooCount++
        let currentFooCount = fooCount

        stopAudio()
        audioDataUrls = null

        if (usePrevNode && prevNode) {
            node = prevNode
        }
        prevNode = node

        let hasChildNodeBlock = function (node) {
            let foo = function (node) {
                if (htmlTagsInlineText.indexOf(node.nodeName) === -1 && htmlTagsInlineIgnore.indexOf(node.nodeName) === -1) {
                    return true
                }

                for (const child of node.childNodes) {
                    if (foo(child)) {
                        return true
                    }
                }
            }
            for (const child of node.childNodes) {
                if (foo(child)) {
                    return true
                }
            }
        }

        if (htmlTagsInlineText.indexOf(node.nodeName) === -1 && htmlTagsInlineIgnore.indexOf(node.nodeName) === -1) {
            if (hasChildNodeBlock(node)) return;
        }

        let text
        if (node.nodeName === "INPUT" || node.nodeName === "TEXTAREA") {
            text = node.placeholder
            if (node.nodeName === "INPUT" && (node.type === "BUTTON" || node.type === "SUBMIT")) {
                text = node.value
                if (!text && node.type === "SUBMIT") {
                    text = "Submit Query"
                }
            }
        } else {
            do {
                if (htmlTagsNoTranslate.indexOf(node.nodeName) !== -1) return;
                if (htmlTagsInlineText.indexOf(node.nodeName) === -1 && htmlTagsInlineIgnore.indexOf(node.nodeName) === -1) {
                    break
                } else {
                    node = node.parentNode
                }
            } while (node && node !== document.body)

            if (!node) return;
            if (node.textContent.length > 1000) return;
            text = node.innerText
        }

        if (!text || text.length < 1 || text.length > 1000) return;

        backgroundTranslateSingleText(currentTextTranslatorService, currentTargetLanguage, text)
            .then(result => {
                if (!result) return;
                if (currentFooCount !== fooCount) return;

                if (!usePrevNode) {
                    init()
                }

                const eTextTranslated = shadowRoot.getElementById("eTextTranslated")
                if (twpLang.isRtlLanguage(currentTargetLanguage)) {
                    eTextTranslated.setAttribute("dir", "rtl")
                } else {
                    eTextTranslated.setAttribute("dir", "ltr")
                }
                eTextTranslated.textContent = result

                const eDivResult = shadowRoot.getElementById("eDivResult")

                const height = eDivResult.offsetHeight
                let top = mousePos.y + 10
                top = Math.max(0, top)
                top = Math.min(window.innerHeight - height, top)

                const width = eDivResult.offsetWidth
                let left = parseInt(mousePos.x /*- (width / 2) */ )
                left = Math.max(0, left)
                left = Math.min(window.innerWidth - width, left)

                if (!usePrevNode) {
                    eDivResult.style.top = top + "px"
                    eDivResult.style.left = left + "px"
                }
            })
            .catch(e => {
                destroy()
            })
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

    function init() {
        destroy()
        if (window.isTranslatingSelected) return;

        divElement = document.createElement("div")
        divElement.style = "all: initial"
        divElement.classList.add("notranslate")

        shadowRoot = divElement.attachShadow({
            mode: "closed"
        })
        shadowRoot.innerHTML = `
        <link rel="stylesheet" href="${chrome.runtime.getURL("/contentScript/css/showTranslated.css")}">

        <div id="eDivResult">
                <div id="eTextTranslated" dir="auto"></div>
                <hr>
                <div id="drag">
                    <ul id="setTargetLanguage">
                        <li value="en" title="English">en</li>
                        <li value="es" title="Spanish">es</li>
                        <li value="de" title="German">de</li>
                    </ul>
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
            shadowRoot.insertBefore(style, shadowRoot.getElementById("eDivResult"))
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
                li:hover {
                    color: rgb(231, 230, 228) !important;
                    background-color: #454a4d !important;
                }
                .selected {
                    background-color: #454a4d !important;
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

        eTextTranslated = shadowRoot.getElementById("eTextTranslated")

        const sGoogle = shadowRoot.getElementById("sGoogle")
        const sYandex = shadowRoot.getElementById("sYandex")
        const sBing = shadowRoot.getElementById("sBing")
        const sDeepL = shadowRoot.getElementById("sDeepL")

        sGoogle.onclick = () => {
            currentTextTranslatorService = "google"
            twpConfig.set("textTranslatorService", "google")
            translateThisNode(null, true)

            sGoogle.classList.remove("selected")
            sYandex.classList.remove("selected")
            sBing.classList.remove("selected")
            sDeepL.classList.remove("selected")

            sGoogle.classList.add("selected")
        }
        sYandex.onclick = () => {
            currentTextTranslatorService = "yandex"
            twpConfig.set("textTranslatorService", "yandex")
            translateThisNode(null, true)

            sGoogle.classList.remove("selected")
            sYandex.classList.remove("selected")
            sBing.classList.remove("selected")
            sDeepL.classList.remove("selected")

            sYandex.classList.add("selected")
        }
        sBing.onclick = () => {
            currentTextTranslatorService = "bing"
            twpConfig.set("textTranslatorService", "bing")
            translateThisNode(null, true)

            sGoogle.classList.remove("selected")
            sYandex.classList.remove("selected")
            sBing.classList.remove("selected")
            sDeepL.classList.remove("selected")

            sBing.classList.add("selected")
        }
        sDeepL.onclick = () => {
            currentTextTranslatorService = "deepl"
            twpConfig.set("textTranslatorService", "deepl")
            translateThisNode(null, true)


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
                    translateThisNode(null, true)
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

        document.body.appendChild(divElement)

        chrome.i18n.translateDocument(shadowRoot)

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

        // if (twpConfig.get("enableDeepL") === "yes") {
        //     sDeepL.removeAttribute("hidden")
        // } else {
        //     sDeepL.setAttribute("hidden", "")
        // }
        twpConfig.onChanged((name, newvalue) => {
            switch (name) {
                case "enableDeepL":
                    // if (newvalue === "yes") {
                    //     sDeepL.removeAttribute("hidden")
                    // } else {
                    //     sDeepL.setAttribute("hidden", "")
                    // }
                    break
            }
        })
    }

    function destroy() {
        fooCount++
        stopAudio()
        audioDataUrls = null

        clearTimeout(timeoutHandler)

        if (divElement) {
            divElement.remove()
            divElement = shadowRoot = eTextTranslated = null
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
        if (!translateTextOverMouseWhenPressTwice) return;
        if (e.key == "Control") {
            if (lastTimePressedCtrl && performance.now() - lastTimePressedCtrl < 280 && !isSelectingText()) {
                lastTimePressedCtrl = performance.now()

                const elements = document.querySelectorAll(":hover")
                if (elements.length > 0) {
                    destroy()
                    translateThisNode(elements[elements.length - 1])
                }
            }
            lastTimePressedCtrl = performance.now()
        }
    }

    function updateEventListener() {
        if (platformInfo.isMobile.any || pageLanguageState == "translated" || !(showTranslatedTextWhenHoveringThisSite || showTranslatedTextWhenHoveringThisLang || translateTextOverMouseWhenPressTwice)) {
            window.removeEventListener("scroll", onScroll)

            window.removeEventListener("mousemove", onMouseMove)
            window.removeEventListener("mousedown", onMouseDown)

            document.removeEventListener("blur", destroy)
            document.removeEventListener("visibilitychange", destroy)

            document.removeEventListener("keyup", onKeyUp)

            destroy()
        } else {
            window.addEventListener("scroll", onScroll)

            window.addEventListener("mousemove", onMouseMove)
            window.addEventListener("mousedown", onMouseDown)

            document.addEventListener("blur", destroy)
            document.addEventListener("visibilitychange", destroy)

            document.addEventListener("keyup", onKeyUp)
        }
    }
    updateEventListener()

    pageTranslator.onGetOriginalTabLanguage(function (tabLanguage) {
        originalTabLanguage = tabLanguage
        showTranslatedTextWhenHoveringThisLang = twpConfig.get("langsToTranslateWhenHovering").indexOf(originalTabLanguage) !== -1
        updateEventListener()
    })

    pageTranslator.onPageLanguageStateChange(_pageLanguageState => {
        pageLanguageState = _pageLanguageState
        updateEventListener()
    })
})
