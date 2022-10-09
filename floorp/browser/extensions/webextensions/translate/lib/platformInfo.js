"use strict";

var platformInfo = {}

twpConfig.onReady(function () {
    if (chrome.tabs) {
        twpConfig.set("originalUserAgent", navigator.userAgent)
    }

    let userAgent
    if (twpConfig.get("originalUserAgent")) {
        userAgent = twpConfig.get("originalUserAgent")
    } else {
        userAgent = navigator.userAgent
    }

    platformInfo.isMobile = {
        Android: userAgent.match(/Android/i),
        BlackBerry: userAgent.match(/BlackBerry/i),
        iOS: userAgent.match(/iPhone|iPad|iPod/i),
        Opera: userAgent.match(/Opera Mini/i),
        Windows: userAgent.match(/IEMobile/i) || userAgent.match(/WPDesktop/i)
    }
    platformInfo.isMobile.any = platformInfo.isMobile.Android || platformInfo.isMobile.BlackBerry || platformInfo.isMobile.iOS || platformInfo.isMobile.Opera || platformInfo.isMobile.Windows

    platformInfo.isDesktop = {
        any: !platformInfo.isMobile.any
    }
})
