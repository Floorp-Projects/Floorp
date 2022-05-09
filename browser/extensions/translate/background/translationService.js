"use strict";

//TODO dividir em varios requests
//TODO Especificar o source lang com page no idioma do paragrafo (dividindo as requests)

const translationService = {};

{
    const googleTranslateTKK = "448487.932609646";
    
    function escapeHTML(unsafe) {
        return unsafe
            .replace(/\&/g, "&amp;")
            .replace(/\</g, "&lt;")
            .replace(/\>/g, "&gt;")
            .replace(/\"/g, "&quot;")
            .replace(/\'/g, "&#39;");
    }

    function unescapeHTML(unsafe) {
        return unsafe
            .replace(/\&amp;/g, "&")
            .replace(/\&lt;/g, "<")
            .replace(/\&gt;/g, ">")
            .replace(/\&quot;/g, "\"")
            .replace(/\&\#39;/g, "'");
    }

    function shiftLeftOrRightThenSumOrXor(num, optString) {
        for (let i = 0; i < optString.length - 2; i += 3) {
            let acc = optString.charAt(i + 2);
            if ('a' <= acc) {
                acc = acc.charCodeAt(0) - 87;
            } else {
                acc = Number(acc);
            }
            if (optString.charAt(i + 1) == '+') {
                acc = num >>> acc;
            } else {
                acc = num << acc;
            }
            if (optString.charAt(i) == '+') {
                num += acc & 4294967295;
            } else {
                num ^= acc;
            }
        }
        return num;
    }

    function transformQuery(query) {
        const bytesArray = [];
        let idx = [];
        for (let i = 0; i < query.length; i++) {
            let charCode = query.charCodeAt(i);
    
            if (128 > charCode) {
                bytesArray[idx++] = charCode;
            } else {
                if (2048 > charCode) {
                    bytesArray[idx++] = charCode >> 6 | 192;
                } else {
                    if (55296 == (charCode & 64512) && i + 1 < query.length && 56320 == (query.charCodeAt(i + 1) & 64512)) {
                        charCode = 65536 + ((charCode & 1023) << 10) + (query.charCodeAt(++i) & 1023);
                        bytesArray[idx++] = charCode >> 18 | 240;
                        bytesArray[idx++] = charCode >> 12 & 63 | 128;
                    } else {
                        bytesArray[idx++] = charCode >> 12 | 224;
                    }
                    bytesArray[idx++] = charCode >> 6 & 63 | 128;
                }
                bytesArray[idx++] = charCode & 63 | 128;
            }

        }
        return bytesArray;
    }

    function calcHash(query, windowTkk) {
        const tkkSplited = windowTkk.split('.');
        const tkkIndex = Number(tkkSplited[0]) || 0;
        const tkkKey = Number(tkkSplited[1]) || 0;
    
        const bytesArray = transformQuery(query);
    
        let encondingRound = tkkIndex;
        for (const item of bytesArray) {
            encondingRound += item;
            encondingRound = shiftLeftOrRightThenSumOrXor(encondingRound, '+-a^+6');
        }
        encondingRound = shiftLeftOrRightThenSumOrXor(encondingRound, '+-3^+b+-f');

        encondingRound ^= tkkKey;
        if (encondingRound <= 0) {
            encondingRound = (encondingRound & 2147483647) + 2147483648;
        }
    
        const normalizedResult = encondingRound % 1000000;
        return normalizedResult.toString() + '.' + (normalizedResult ^ tkkIndex);
    }

    let lastYandexRequestSIDTime = null
    let yandexTranslateSID = null;
    let yandexSIDNotFound = false
    let yandexGetSidPromise = null
    async function getYandexSID() {
        if (yandexGetSidPromise) {
            return yandexGetSidPromise;
        }

        yandexGetSidPromise = new Promise(resolve => {
            let updateYandexSid = false
            if (lastYandexRequestSIDTime) {
                const date = new Date();
                if (yandexTranslateSID) {
                    date.setHours(date.getHours() - 12)
                } else if (yandexSIDNotFound) {
                    date.setMinutes(date.getMinutes() - 30)
                } else {
                    date.setMinutes(date.getMinutes() - 2)
                }
                if (date.getTime() > lastYandexRequestSIDTime) {
                    updateYandexSid = true
                }
            } else {
                updateYandexSid = true
            }

            if (updateYandexSid) {
                lastYandexRequestSIDTime = Date.now()

                const http = new XMLHttpRequest
                http.open("GET", "https://translate.yandex.net/website-widget/v1/widget.js?widgetId=ytWidget&pageLang=es&widgetTheme=light&autoMode=false")
                http.send()
                http.onload = e => {
                    const result = http.responseText.match(/sid\:\s\'[0-9a-f\.]+/)
                    if (result && result[0] && result[0].length > 7) {
                        yandexTranslateSID = result[0].substring(6)
                        yandexSIDNotFound = false
                    } else {
                        yandexSIDNotFound = true
                    }
                    resolve()
                }
                http.onerror = e => {
                    console.error(e)
                    resolve()
                }
            } else {
                resolve()
            }
        })

        yandexGetSidPromise.finally(() => {
            yandexGetSidPromise = null
        })

        return await yandexGetSidPromise
    }

    let lastBingRequestSIDTime = null
    let bingTranslateSID = null;
    let bingSIDNotFound = false
    let bingGetSidPromise = null
    async function getBingSID() {
        if (bingGetSidPromise) {
            return bingGetSidPromise;
        }

        bingGetSidPromise = new Promise(resolve => {
            let updateBingSid = false
            if (lastBingRequestSIDTime) {
                const date = new Date();
                if (bingTranslateSID) {
                    date.setHours(date.getHours() - 12)
                } else if (bingSIDNotFound) {
                    date.setMinutes(date.getMinutes() - 30)
                } else {
                    date.setMinutes(date.getMinutes() - 2)
                }
                if (date.getTime() > lastBingRequestSIDTime) {
                    updateBingSid = true
                }
            } else {
                updateBingSid = true
            }

            if (updateBingSid) {
                lastBingRequestSIDTime = Date.now()

                const http = new XMLHttpRequest
                http.open("GET", "https://www.bing.com/translator")
                http.send()
                http.onload = e => {
                    const result = http.responseText.match(/params_RichTranslateHelper\s=\s\[[^\]]+/)
                    if (result && result[0] && result[0].length > 50) {
                        const params_RichTranslateHelper = result[0].substring("params_RichTranslateHelper = [".length).split(",")
                        if (params_RichTranslateHelper && params_RichTranslateHelper[0] && params_RichTranslateHelper[1] && parseInt(params_RichTranslateHelper[0])) {
                            bingTranslateSID = `&token=${params_RichTranslateHelper[1].substring(1, params_RichTranslateHelper[1].length-1)}&key=${parseInt(params_RichTranslateHelper[0])}`
                            bingSIDNotFound = false
                        } else {
                            bingSIDNotFound = true
                        }
                    } else {
                        bingSIDNotFound = true
                    }
                    resolve()
                }
                http.onerror = e => {
                    console.error(e)
                    resolve()
                }
            } else {
                resolve()
            }
        })

        bingGetSidPromise.finally(() => {
            bingGetSidPromise = null
        })

        return await bingGetSidPromise
    }

    const googleTranslationInProgress = {}
    const yandexTranslationInProgress = {}
    const bingTranslationInProgress = {}

    function getTranslationInProgress(translationService, targetLanguage) {
        let translationInProgress
        if (translationService === "yandex") {
            translationInProgress = yandexTranslationInProgress
        } else if (translationInProgress === "bing") {
            translationInProgress = bingTranslationInProgress
        } else {
            translationInProgress = googleTranslationInProgress
        }

        if (!translationInProgress[targetLanguage]) {
            translationInProgress[targetLanguage] = []
        }

        return translationInProgress[targetLanguage]
    }

    translationService.google = {}
    translationService.yandex = {}
    translationService.bing = {}
    translationService.deepl = {}

    async function translateHTML(translationService, targetLanguage, translationServiceURL, sourceArray, requestBody, textParamName, translationProgress, dontSaveInCache = false) {
        const thisTranslationProgress = []
        const requests = []

        for (const str of sourceArray) {
            const transInfo = translationProgress.find(value => value.source === str)
            if (transInfo) {
                thisTranslationProgress.push(transInfo)
            } else {
                let translated
                try {
                    translated = await translationCache.get(translationService, str, targetLanguage)
                } catch (e) {
                    console.error(e)
                }
                let newTransInfo
                if (translated) {
                    newTransInfo = {
                        source: str,
                        translated,
                        status: "complete"
                    }
                } else {
                    newTransInfo = {
                        source: str,
                        translated: null,
                        status: "translating"
                    }

                    if (requests.length < 1 || requests[requests.length - 1].requestBody.length > 800) {
                        requests.push({
                            requestBody,
                            fullSource: "",
                            transInfos: []
                        })
                    }

                    requests[requests.length - 1].requestBody += "&" + textParamName + "=" + encodeURIComponent(str)
                    requests[requests.length - 1].fullSource += str
                    requests[requests.length - 1].transInfos.push(newTransInfo)
                }

                translationProgress.push(newTransInfo)
                thisTranslationProgress.push(newTransInfo)
            }
        }

        if (requests.length > 0) {
            for (const request of requests) {
                let tk = ""
                if (translationService === "google") {
                    tk = calcHash(request.fullSource, googleTranslateTKK)
                }

                const http = new XMLHttpRequest
                if (translationService === "google") {
                    http.open("POST", translationServiceURL + tk)
                    http.setRequestHeader("Content-Type", "application/x-www-form-urlencoded")
                    http.responseType = "json"
                    http.send(request.requestBody)
                } else if (translationService === "yandex") {
                    http.open("GET", translationServiceURL + request.requestBody)
                    http.setRequestHeader("Content-Type", "application/x-www-form-urlencoded")
                    http.responseType = "json"
                    http.send(request.requestBody)
                } else if (translationService === "bing") {
                    http.open("POST", "https://www.bing.com/ttranslatev3?isVertical=1")
                    http.setRequestHeader("Content-Type", "application/x-www-form-urlencoded")
                    http.responseType = "json"
                    http.send(`&fromLang=auto-detect${request.requestBody}&to=${targetLanguage}${bingTranslateSID}`)
                }

                http.onload = e => {
                    try {
                        const response = http.response
                        let responseJson
                        if (translationService === "yandex") {
                            responseJson = response.text
                        } else if (translationService === "google") {
                            if (typeof response[0] == "string") {
                                responseJson = response
                            } else {
                                responseJson = response.map(value => value[0])
                            }
                        } else if (translationService === "bing") {
                            responseJson = [http.response[0].translations[0].text]
                        }

                        request.transInfos.forEach((transInfo, index) => {
                            try {
                                if (responseJson[index]) {
                                    transInfo.status = "complete"
                                    transInfo.translated = responseJson[index]

                                    if (!dontSaveInCache) {
                                        try {
                                            //TODO ERRO AQUI FAZ DA LENTIDAO
                                            translationCache.set(translationService, transInfo.source, transInfo.translated, targetLanguage)
                                        } catch (e) {
                                            console.error(e)
                                        }
                                    }
                                } else {
                                    transInfo.status = "error"
                                }
                            } catch (e) {
                                transInfo.status = "error"
                                console.error(e)
                            }
                        })
                        return responseJson
                    } catch (e) {
                        console.error(e)

                        request.transInfos.forEach((transInfo, index) => {
                            transInfo.status = "error"
                        })
                    }
                }
                http.onerror = e => {
                    request.transInfos.forEach(transInfo => {
                        transInfo.status = "error"
                    })
                    console.error(e)
                }
            }
        }

        const promise = new Promise((resolve, reject) => {
            let iterationsCount = 0

            function waitForTranslationFinish() {
                let isTranslating = false
                for (const info of thisTranslationProgress) {
                    if (info.status === "translating") {
                        isTranslating = true
                        break
                    }
                }

                if (++iterationsCount < 100) {
                    if (isTranslating) {
                        setTimeout(waitForTranslationFinish, 100)
                    } else {
                        resolve(thisTranslationProgress)
                        return
                    }
                } else {
                    reject()
                    return
                }
            }
            waitForTranslationFinish()
        })

        try {
            return await promise
        } catch (e) {
            console.error(e)
        }
    }

    // nao funciona bem por problemas em detectar o idioma do texto
    async function fixSouceArray(sourceArray3d) {
        const newSourceArray3d = []
        const fixIndexesMap = []

        for (const i in sourceArray3d) {
            newSourceArray3d.push([])
            fixIndexesMap.push(parseInt(i))

            const sourceArray = sourceArray3d[i]
            let prevDetectedLanguage = null
            for (const j in sourceArray) {
                const text = sourceArray[j]
                const detectedLanguage = await new Promise(resolve => {
                    chrome.i18n.detectLanguage(text, result => {
                        if (result && result.languages && result.languages.length > 0) {
                            resolve(result.languages[Object.keys(result.languages)[0]].language)
                        } else {
                            resolve(null)
                        }
                    })
                })
                if (detectedLanguage && prevDetectedLanguage && detectedLanguage !== prevDetectedLanguage && newSourceArray3d[newSourceArray3d.length - 1].length > 0) {
                    newSourceArray3d.push([text])
                    fixIndexesMap.push(parseInt(i))
                } else {
                    newSourceArray3d[newSourceArray3d.length - 1].push(text)
                }
                prevDetectedLanguage = detectedLanguage
            }
        }

        return [newSourceArray3d, fixIndexesMap]
    }

    function fixResultArray(resultArray3d, fixIndexesMap) {
        const newResultArray3d = []

        let idx = 0
        for (const index of fixIndexesMap) {
            if (!newResultArray3d[index]) {
                newResultArray3d[index] = []
            }
            if (resultArray3d[idx]) {
                for (const text of resultArray3d[idx]) {
                    newResultArray3d[index].push(text)
                }
                idx++
            } else {
                console.error("resultArray is undefined")
                break
            }
        }

        if (newResultArray3d[newResultArray3d.length - 1].length < 1) {
            newResultArray3d.pop()
        }

        return newResultArray3d
    }

    // async para fix
    translationService.google.translateHTML = (_sourceArray3d, targetLanguage, dontSaveInCache = false, preseveTextFormat = false, dontSortResults = false) => {
        if (targetLanguage == "zh") {
            targetLanguage = "zh-CN"
        }

        //const [sourceArray3d, fixIndexesMap] = await fixSouceArray(_sourceArray3d)
        const sourceArray = _sourceArray3d.map(sourceArray => {
            sourceArray = sourceArray.map(value => escapeHTML(value))
            if (sourceArray.length > 1) {
                sourceArray = sourceArray.map((value, index) => "<a i=" + index + ">" + value + "</a>")
            }
            //if (preseveTextFormat) {
            return "<pre>" + sourceArray.join("") + "</pre>"
            //}
            //return sourceArray.join("")
        })

        const requestBody = ""
        return translateHTML(
                "google",
                targetLanguage,
                `https://translate.googleapis.com/translate_a/t?anno=3&client=te&v=1.0&format=html&sl=auto&tl=` + targetLanguage + "&tk=",
                sourceArray,
                requestBody,
                "q",
                getTranslationInProgress("google", targetLanguage),
                dontSaveInCache
            )
            .then(thisTranslationProgress => {
                const results = thisTranslationProgress.map(value => value.translated)
                const resultArray3d = []

                for (const i in results) {
                    let result = results[i]
                    if (result.indexOf("<pre") !== -1) {
                        result = result.replace("</pre>", "")
                        const index = result.indexOf(">")
                        result = result.slice(index + 1)
                    }
                    const sentences = []

                    let idx = 0
                    while (true) {
                        const sentenceStartIndex = result.indexOf("<b>", idx)
                        if (sentenceStartIndex === -1) break;

                        const sentenceFinalIndex = result.indexOf("<i>", sentenceStartIndex)

                        if (sentenceFinalIndex === -1) {
                            sentences.push(result.slice(sentenceStartIndex + 3))
                            break
                        } else {
                            sentences.push(result.slice(sentenceStartIndex + 3, sentenceFinalIndex))
                        }
                        idx = sentenceFinalIndex
                    }

                    result = sentences.length > 0 ? sentences.join(" ") : result
                    let resultArray = result.match(/\<a\si\=[0-9]+\>[^\<\>]*(?=\<\/a\>)/g)

                    if (dontSortResults) {
                        // Should not sort the <a i={number}> of Google Translate result
                        // Instead of it, join the texts without sorting
                        // https://github.com/FilipePS/Traduzir-paginas-web/issues/163

                        if (resultArray && resultArray.length > 0) {
                            resultArray = resultArray.map(value => {
                                const resultStartAtIndex = value.indexOf('>');
                                return value.slice(resultStartAtIndex + 1)
                            })
                        } else {
                            resultArray = [result]
                        }

                        resultArray = resultArray.map(value => value.replace(/\<\/b\>/g, ""))
                        resultArray = resultArray.map(value => unescapeHTML(value))

                        resultArray3d.push(resultArray)
                    } else {
                        let indexes
                        if (resultArray && resultArray.length > 0) {
                            indexes = resultArray.map(value => parseInt(value.match(/[0-9]+(?=\>)/g))).filter(value => !isNaN(value))
                            resultArray = resultArray.map(value => {
                                const resultStartAtIndex = value.indexOf('>');
                                return value.slice(resultStartAtIndex + 1)
                            })
                        } else {
                            resultArray = [result]
                            indexes = [0]
                        }

                        resultArray = resultArray.map(value => value.replace(/\<\/b\>/g, ""))
                        resultArray = resultArray.map(value => unescapeHTML(value))

                        const finalResulArray = []
                        for (const j in indexes) {
                            if (finalResulArray[indexes[j]]) {
                                finalResulArray[indexes[j]] += " " + resultArray[j]
                            } else {
                                finalResulArray[indexes[j]] = resultArray[j]
                            }
                        }

                        resultArray3d.push(finalResulArray)
                    }
                }

                //return fixResultArray(resultArray3d, fixIndexesMap)
                return resultArray3d
            })
    }
    
    translationService.google.translateText = async (sourceArray, targetLanguage, dontSaveInCache = false) => {
        if (targetLanguage == "zh") {
            targetLanguage = "zh-CN"
        }

        return (await translationService.google.translateHTML(sourceArray.map(value => [value]), targetLanguage, dontSaveInCache, true)).map(value => value[0])
    }
    
    translationService.google.translateSingleText = (source, targetLanguage, dontSaveInCache = false) => translationService.google.translateText([ source ], targetLanguage, dontSaveInCache).then(results => results[0])
    
    translationService.yandex.translateHTML = async (sourceArray3d, targetLanguage, dontSaveInCache = false) => {
        await getYandexSID()
        if (!yandexTranslateSID) return

        if (targetLanguage.indexOf("zh-") !== -1) {
            targetLanguage = "zh"
        }

        const sourceArray = sourceArray3d.map(sourceArray =>
            sourceArray.map(value => escapeHTML(value)).join("<wbr>"))

        const requestBody = "format=html&lang=" + targetLanguage
        return await translateHTML(
                "yandex",
                targetLanguage,
                "https://translate.yandex.net/api/v1/tr.json/translate?srv=tr-url-widget&id=" + yandexTranslateSID + "-0-0&",
                sourceArray,
                requestBody,
                "text",
                getTranslationInProgress("yandex", targetLanguage),
                dontSaveInCache
            )
            .then(thisTranslationProgress => {
                const results = thisTranslationProgress.map(value => value.translated)

                const resultArray3d = []
                for (const result of results) {
                    resultArray3d.push(
                        result
                        .split("<wbr>")
                        .map(value => unescapeHTML(value))
                    )
                }

                return resultArray3d
            })
    }
    
    translationService.yandex.translateText = async (sourceArray, targetLanguage, dontSaveInCache = false) => {
        if (targetLanguage.indexOf("zh-") !== -1) {
            targetLanguage = "zh"
        }

        return (await translationService.yandex.translateHTML(sourceArray.map(value => [value]), targetLanguage, dontSaveInCache)).map(value => value[0])
    }
    
    translationService.yandex.translateSingleText = (source, targetLanguage, dontSaveInCache = false) => translationService.yandex.translateText([ source ], targetLanguage, dontSaveInCache).then(results => results[0])
    
    translationService.bing.translateSingleText = async (source, targetLanguage, dontSaveInCache = false) => {
        if (targetLanguage == "zh-CN") {
            targetLanguage = "zh-Hans"
        } else if (targetLanguage == "zh-TW") {
            targetLanguage = "zh-Hant"
        } else if (targetLanguage == "tl") {
            targetLanguage = "fil"
        } else if (targetLanguage.indexOf("zh-") !== -1) {
            targetLanguage = "zh-Hans"
        }
        await getBingSID()

        return await translateHTML(
                "bing",
                targetLanguage,
                "https://www.bing.com/ttranslatev3?isVertical=1",
                [source],
                "",
                "text",
                getTranslationInProgress("bing", targetLanguage),
                dontSaveInCache
            )
            .then(thisTranslationProgress => thisTranslationProgress[0].translated)
    }
    
    let DeepLTab = null;
    translationService.deepl.translateSingleText = (source, targetlanguage, dontSaveInCache = false) => {
        //*
        return new Promise(resolve => {
            function waitFirstTranslationResult() {
                function listener(request, sender, sendResponse) {
                    if (request.action === "DeepL_firstTranslationResult") {
                        resolve(request.result)
                        chrome.runtime.onMessage.removeListener(listener)
                    }
                }
                chrome.runtime.onMessage.addListener(listener)

                setTimeout(() => {
                    chrome.runtime.onMessage.removeListener(listener)
                    resolve("")
                }, 8000)
            }

            if (DeepLTab) {
                chrome.tabs.get(DeepLTab.id, tab => {
                    checkedLastError()
                    if (tab) {
                        //chrome.tabs.update(tab.id, {active: true})
                        chrome.tabs.sendMessage(tab.id, {
                            action: "translateTextWithDeepL",
                            text: source,
                            targetlanguage
                        }, {
                            frameId: 0
                        }, response => resolve(response))
                    } else {
                        chrome.tabs.create({
                            url: `https://www.deepl.com/#!${targetlanguage}!#${encodeURIComponent(source)}`
                        }, tab => {
                            DeepLTab = tab
                            waitFirstTranslationResult()
                        })
                        //resolve("")
                    }
                })
            } else {
                chrome.tabs.create({
                    url: `https://www.deepl.com/#!${targetlanguage}!#${encodeURIComponent(source)}`
                }, tab => {
                    DeepLTab = tab
                    waitFirstTranslationResult()
                })
                //resolve("")
            }
        })
        //*/
        /*
        return new Promise((resolve, reject) => {
            const request = {
                "jsonrpc": "2.0",
                "method": "LMT_handle_jobs",
                "params": {
                    "jobs": [{
                        "kind": "default",
                        "raw_en_sentence": source,
                        "raw_en_context_before": [],
                        "raw_en_context_after": [],
                        "preferred_num_beams": 4
                    }],
                    "lang": {
                        "user_preferred_langs": [targetlanguage.toUpperCase(), "EN"],
                        "target_lang": targetlanguage.toUpperCase()
                    },
                    "priority": -1,
                    "commonJobParams": { },
                    "timestamp": Date.now()
                },
                "id": 0
            }
    
            const http = new XMLHttpRequest
            http.open("POST", "https://www2.deepl.com/jsonrpc")
            http.setRequestHeader("Content-Type", "application/json")
            http.responseType = "json"
    
            http.send(JSON.stringify(request).replace(`"method":`, `"method": `))
    
            http.onload = e => {
                const response = http.response
                try {
                    const translated = response.result.translations[0].beams[0].postprocessed_sentence
                    resolve(translated)
                } catch (e) {
                    reject(e)
                }
            }
    
            http.onerror = e => {
                console.error(e)
                reject(e)
            }
        })
        //*/
    }
    
    
    chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
        if (request.action === "translateHTML") {
            let translateHTML
            if (request.translationService === "yandex") {
                translateHTML = translationService.yandex.translateHTML
            } else {
                translateHTML = translationService.google.translateHTML
            }

            translateHTML(request.sourceArray3d, request.targetLanguage, sender.tab ? sender.tab.incognito : false, true, request.dontSortResults)
                .then(results => sendResponse(results))
                .catch(e => sendResponse())

            return true
        } else if (request.action === "translateText") {
            let translateText
            if (request.translationService === "yandex") {
                translateText = translationService.yandex.translateText
            } else {
                translateText = translationService.google.translateText
            }

            translateText(request.sourceArray, request.targetLanguage, sender.tab ? sender.tab.incognito : false)
                .then(results => sendResponse(results))
                .catch(e => sendResponse())

            return true
        } else if (request.action === "translateSingleText") {
            let translateSingleText
            if (request.translationService === "deepl") {
                translateSingleText = translationService.deepl.translateSingleText
            } else if (request.translationService === "yandex") {
                translateSingleText = translationService.yandex.translateSingleText
            } else if (request.translationService === "bing") {
                translateSingleText = translationService.bing.translateSingleText
            } else {
                translateSingleText = translationService.google.translateSingleText
            }

            translateSingleText(request.source, request.targetLanguage, sender.tab ? sender.tab.incognito : false)
                .then(result => sendResponse(result))
                .catch(e => sendResponse())

            return true
        }
    })
}
