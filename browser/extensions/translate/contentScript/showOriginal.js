"use strict";

//TODO mostrar o texto original apenas da frase sobre o mouse

var showOriginal = {}

twpConfig.onReady(function () {
    if (platformInfo.isMobile.any) {
        showOriginal.enable = () => {}
        showOriginal.disable = () => {}
        showOriginal.add = () => {}
        showOriginal.removeAll = () => {}
        return;
    }

    let styleTextContent = ""
    fetch(chrome.runtime.getURL("/contentScript/css/showOriginal.css"))
        .then(response => response.text())
        .then(response => styleTextContent = response)
        .catch(e => console.error(e))

    let showOriginalTextWhenHovering = twpConfig.get("showOriginalTextWhenHovering")
    twpConfig.onChanged(function (name, newValue) {
        if (name === "showOriginalTextWhenHovering") {
            showOriginalTextWhenHovering = newValue
            showOriginal.enable(true)
        }
    })

    let originalTextIsShowing = false
    let divElement
    let shadowRoot
    let currentNodeOverMouse
    let timeoutHandler

    let nodesToShowOriginal = []

    const mousePos = {
        x: 0,
        y: 0
    }

    function onMouseMove(e) {
        mousePos.x = e.clientX
        mousePos.y = e.clientY
    }

    function onMouseDown(e) {
        if (!divElement) return;
        if (e.target === divElement) return;
        hideOriginalText()
    }

    function showOriginalText(node) {
        hideOriginalText()
        if (!divElement) return;
        if (window.isTranslatingSelected) return;

        const nodeInf = nodesToShowOriginal.find(nodeInf => nodeInf.node === node)
        if (nodeInf) {
            const eOriginalText = shadowRoot.getElementById("originalText")
            eOriginalText.textContent = nodeInf.original
            document.body.appendChild(divElement)
            originalTextIsShowing = true

            const height = eOriginalText.offsetHeight
            let top = mousePos.y + 10
            top = Math.max(0, top)
            top = Math.min(window.innerHeight - height, top)

            const width = eOriginalText.offsetWidth
            let left = parseInt(mousePos.x /*- (width / 2) */ )
            left = Math.max(0, left)
            left = Math.min(window.innerWidth - width, left)

            eOriginalText.style.top = top + "px"
            eOriginalText.style.left = left + "px"
        }
    }

    function hideOriginalText() {
        if (divElement) {
            divElement.remove()
            originalTextIsShowing = false
        }
        clearTimeout(timeoutHandler)
    }

    function isShowingOriginalText() {
        return originalTextIsShowing
    }

    function onMouseEnter(e) {
        if (!divElement) return;
        if (currentNodeOverMouse && e.target === currentNodeOverMouse) return;
        currentNodeOverMouse = e.target
        if (timeoutHandler) clearTimeout(timeoutHandler);
        timeoutHandler = setTimeout(showOriginalText, 1500, currentNodeOverMouse)
    }

    function onMouseOut(e) {
        if (!divElement) return;
        if (!isShowingOriginalText()) return;

        if (e.target === currentNodeOverMouse && e.relatedTarget === divElement) return;
        if (e.target === divElement && e.relatedTarget === currentNodeOverMouse) return;

        hideOriginalText()
    }

    showOriginal.add = function (node) {
        if (platformInfo.isMobile.any) return;

        if (node && nodesToShowOriginal.indexOf(node) === -1) {
            nodesToShowOriginal.push({
                node: node,
                original: node.textContent
            })
            node.addEventListener("mouseenter", onMouseEnter)
            node.addEventListener("mouseout", onMouseOut)
        }
    }

    showOriginal.removeAll = function () {
        nodesToShowOriginal.forEach(nodeInf => {
            nodeInf.node.removeEventListener("mouseenter", onMouseEnter)
            nodeInf.node.removeEventListener("mouseout", onMouseOut)
        })
        nodesToShowOriginal = []
    }

    showOriginal.enable = function (dontDeleteNodesToShowOriginal = false) {
        showOriginal.disable(dontDeleteNodesToShowOriginal)

        if (platformInfo.isMobile.any) return;
        if (showOriginalTextWhenHovering !== "yes") return;
        if (divElement) return;

        divElement = document.createElement("div")
        divElement.style = "all: initial"
        divElement.classList.add("notranslate")

        shadowRoot = divElement.attachShadow({
            mode: "closed"
        })
        shadowRoot.innerHTML = `
            <link rel="stylesheet" href="${chrome.runtime.getURL("/contentScript/css/showOriginal.css")}">
            <div id="originalText" dir="auto"></div>
        `

        {
            const style = document.createElement("style")
            style.textContent = styleTextContent
            shadowRoot.insertBefore(style, shadowRoot.getElementById("originalText"))
        }

        function enableDarkMode() {
            if (!shadowRoot.getElementById("darkModeElement")) {
                const el = document.createElement("style")
                el.setAttribute("id", "darkModeElement")
                el.setAttribute("rel", "stylesheet")
                el.textContent = `
                    * {
                        scrollbar-color: #202324 #454a4d;
                    }
                    #originalText {
                        color: rgb(231, 230, 228) !important;
                        background-color: #181a1b !important;
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

        divElement.addEventListener("mouseout", onMouseOut)

        document.addEventListener("mousemove", onMouseMove)
        document.addEventListener("mousedown", onMouseDown)

        document.addEventListener("blur", hideOriginalText)
        document.addEventListener("visibilitychange", hideOriginalText)
    }

    showOriginal.disable = function (dontDeleteNodesToShowOriginal = false) {
        if (divElement) {
            hideOriginalText()
            divElement.remove()
            divElement = null
            shadowRoot = null
        }

        if (!dontDeleteNodesToShowOriginal) {
            showOriginal.removeAll()
        }

        document.removeEventListener("mousemove", onMouseMove)
        document.removeEventListener("mousedown", onMouseDown)

        document.removeEventListener("blur", hideOriginalText)
        document.removeEventListener("visibilitychange", hideOriginalText)
    }
})
