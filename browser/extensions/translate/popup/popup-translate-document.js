const send = document.getElementById("send")
const error = document.getElementById("error")
const selectService = document.getElementById("selectService")
const pleaseWait = document.getElementById("pleasewait")
const googleTranslate = document.getElementById("googletranslate")
const cannotUseGoogle = document.getElementById("cannotusegoogle")
const cannotDownload = document.getElementById("cannotDownload")
const conversion = document.getElementById("conversion")
const conversionAlert = document.getElementById("conversionalert")

async function showError(e) {
    console.error(e)
    selectService.style.display = "none"
    conversion.style.display = "none"
    conversionAlert.style.display = "none"
    pleaseWait.style.display = "none"
    cannotDownload.style.display = "block"
}

async function downloadDocument(url) {
    return new Promise((resolve, reject) => {
        try {
            const http = new XMLHttpRequest
            http.open("get", url, true)
            http.responseType = "arraybuffer";
            http.onprogress = e => {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    pleaseWait.textContent = chrome.i18n.getMessage("msgPleaseWait") + " " + percentComplete.toFixed(1) + "%"
                }
            }
            http.onload = e => {
                resolve(e.target.response)
            }
            http.onerror = e => {
                showError(e)
                reject(e)
            }
            http.send()
        } catch(e) {
            showError(e)
        }
    })
}

async function convertDocument(service, data) {
    const file = new File([data], "document.pdf",{type:"application/pdf", lastModified: Date.now()});
    const container = new DataTransfer();
    container.items.add(file)
    const myForm = document.getElementById("form_" + service)
    myForm.querySelector('[type="file"]').files = container.files
    if (myForm.querySelector('[name="tl"]')) {
        myForm.querySelector('[name="tl"]').value = twpConfig.get("targetLanguage")
    }
    pleaseWait.style.display = "none"
    send.style.display = "block"
    send.onclick = e => {
        myForm.querySelector('[type="submit"]').click()
        window.close()
    }
}

selectService.onclick = async e => {
    if (e.target.dataset.name == "google" || e.target.dataset.name == "translatewebpages") {
        selectService.style.display = "none"
        pleaseWait.style.display = "block"
        conversion.style.display = "none"
        conversionAlert.style.display = "none"
        cannotUseGoogle.style.display = "none"
        chrome.tabs.query({active: true, currentWindow: true}, async tabs => {
            try {
                const service = e.target.dataset.name
                if (tabs[0].url.startsWith("file:")) {
                    if (service == "google") {
                        chrome.tabs.create({url: "https://translate.google.com/?op=docs"})
                    } else {
                        chrome.tabs.create({url: "https://translatewebpages.org/"})
                    }
                    return window.close()
                }
                const data = await downloadDocument(tabs[0].url)
                pleaseWait.style.display = "none"
                if (data.byteLength > 1024*1024*10 && service == "google") {
                    conversion.style.display = "block"
                    conversionAlert.style.display = "block"
                    googleTranslate.style.display = "none"
                    selectService.style.display = "block"
                    cannotUseGoogle.textContent = chrome.i18n.getMessage("msgFileLargerThan", "10 MB")
                    cannotUseGoogle.style.display = "block"
                } else {
                    convertDocument(service, data)
                }
            } catch(e) {
                showError(e)
            }
        })
    }
}



$ = document.querySelector.bind(document)

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
        .servicebutton, .sendbutton, .title {
            background-color: rgba(0, 0, 0, 0.2);
        }
        
        .servicebutton:hover, .sendbutton:hover {
            background-color: rgba(0, 0, 0, 0.4);
        }
        `
        document.head.appendChild(el)
    }
}

function enableDarkMode() {
    if ($("#lightModeElement")) $("#lightModeElement").remove()
}

twpConfig.onReady(() => {
    switch (twpConfig.get("darkMode")) {
        case "auto":
            if (matchMedia("(prefers-color-scheme: dark)").matches) enableDarkMode()
            else disableDarkMode()
            break
        case "yes":
            enableDarkMode()
            break
        case "no":
            disableDarkMode()
            break
    }
})
