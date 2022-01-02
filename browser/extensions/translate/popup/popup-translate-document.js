const sectionSelect = document.getElementById("sectionSelect")
const sectionDownload = document.getElementById("sectionDownload")

const progress = document.getElementById("progress")
const send = document.getElementById("send")
const tryAgain = document.getElementById("tryAgain")

const selectService = document.getElementById("selectService")
selectService.onclick = e => {
    sectionSelect.style.display = "none"
    sectionDownload.style.display = "block"

    chrome.tabs.query({active: true, currentWindow: true}, async tabs => {
        try {
            const service = e.target.dataset.name
            if (tabs[0].url.startsWith("file:")) {
                if (service == "google") {
                    chrome.tabs.create({url: "https://translate.google.com/?op=docs"})
                } else {
                    chrome.tabs.create({url: "https://translatewebpages.org/"})
                }
                window.close()
                return
            }
            const data = await downloadDocument(tabs[0].url)
            convertDocument(service, data)
        } catch (e) {
            console.error(e)
            showError(chrome.i18n.getMessage("msgErrorDownloadingDocument")) // "Error downloading document"
        }
    })
}

tryAgain.onclick = e => {
    progress.textContent = "0%"
    tryAgain.style.display = "none"
    error.style.display = "none"
    send.style.display = "none"

    sectionDownload.style.display = "none"
    sectionSelect.style.display = "block"
}

function showError(msg) {
    error.textContent = msg
    error.style.display = "block"
    tryAgain.style.display = "block"
}

function downloadDocument(url) {
    return new Promise((resolve, reject) => {
        const http = new XMLHttpRequest
        http.open("get", url, true)
        http.responseType = "arraybuffer";
        http.onprogress = e => {
            if (e.lengthComputable) {
                const percentComplete = (e.loaded / e.total) * 100;
                progress.textContent = percentComplete.toFixed(1) + "%"
            }
        }
        http.onload = e => {
            resolve(e.target.response)
        }
        http.onerror = e => {
            reject(e)
        }
        http.send()
    })
}

function convertDocument(service, data) {
    if (service === "google" && data.byteLength > 1048576) {
        showError(chrome.i18n.getMessage("msgFileLargerThan", "10 MB")) // "This service does not support files larger than 10 MB"
        return
    }

    const file = new File([data], "document.pdf",{type:"application/pdf", lastModified: new Date().getTime()});
    const container = new DataTransfer();
    container.items.add(file)

    const myForm = document.getElementById("form_" + service)
    myForm.querySelector('[type="file"]').files = container.files

    send.style.display = "block"

    send.onclick = e => {
        myForm.querySelector('[type="submit"').click()
        window.close()
    }
}



$ = document.querySelector.bind(document)

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
        `
        document.head.appendChild(el)
    }
}

function disableDarkMode() {
    if ($("#darkModeElement")) {
        $("#darkModeElement").remove()
    }
}

twpConfig.onReady(() => {
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