/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals module, require */

// This is a hack for the tests.
if (typeof InterventionHelpers === "undefined") {
  var InterventionHelpers = require("../lib/intervention_helpers");
}

/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const AVAILABLE_INJECTIONS = [
  {
    id: "testbed-injection",
    platform: "all",
    domain: "webcompat-addon-testbed.herokuapp.com",
    bug: "0000000",
    hidden: true,
    contentScripts: {
      matches: ["*://webcompat-addon-testbed.herokuapp.com/*"],
      css: [
        {
          file: "injections/css/bug0000000-testbed-css-injection.css",
        },
      ],
      js: [
        {
          file: "injections/js/bug0000000-testbed-js-injection.js",
        },
      ],
    },
  },
  {
    id: "bug1452707",
    platform: "desktop",
    domain: "ib.absa.co.za",
    bug: "1452707",
    contentScripts: {
      matches: ["https://ib.absa.co.za/*"],
      js: [
        {
          file:
            "injections/js/bug1452707-window.controllers-shim-ib.absa.co.za.js",
        },
      ],
    },
  },
  {
    id: "bug1457335",
    platform: "desktop",
    domain: "histography.io",
    bug: "1457335",
    contentScripts: {
      matches: ["*://histography.io/*"],
      js: [
        {
          file: "injections/js/bug1457335-histography.io-ua-change.js",
        },
      ],
    },
  },
  {
    id: "bug1472075",
    platform: "desktop",
    domain: "bankofamerica.com",
    bug: "1472075",
    contentScripts: {
      matches: ["*://*.bankofamerica.com/*"],
      js: [
        {
          file: "injections/js/bug1472075-bankofamerica.com-ua-change.js",
        },
      ],
    },
  },
  {
    id: "bug1579159",
    platform: "android",
    domain: "m.tailieu.vn",
    bug: "1579159",
    contentScripts: {
      matches: ["*://m.tailieu.vn/*", "*://m.elib.vn/*"],
      js: [
        {
          file: "injections/js/bug1579159-m.tailieu.vn-pdfjs-worker-disable.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1551672",
    platform: "android",
    domain: "Sites using PDK 5 video",
    bug: "1551672",
    data: {
      urls: ["https://*/*/tpPdk.js", "https://*/*/pdk/js/*/*.js"],
      types: ["script"],
    },
    customFunc: "pdk5fix",
  },
  {
    id: "bug1583366",
    platform: "desktop",
    domain: "Download prompt for files with no content-type",
    bug: "1583366",
    data: {
      urls: ["https://ads-us.rd.linksynergy.com/as.php*"],
      contentType: {
        name: "content-type",
        value: "text/html; charset=utf-8",
      },
    },
    customFunc: "noSniffFix",
  },
  {
    id: "bug1768243",
    platform: "desktop",
    domain: "cloud.google.com",
    bug: "1768243",
    contentScripts: {
      matches: ["*://cloud.google.com/terms/*"],
      css: [
        {
          file:
            "injections/css/bug1768243-cloud.google.com-allow-table-scrolling.css",
        },
      ],
    },
  },
  {
    id: "bug1570328",
    platform: "android",
    domain: "developer.apple.com",
    bug: "1570328",
    contentScripts: {
      matches: ["*://developer.apple.com/*"],
      css: [
        {
          file:
            "injections/css/bug1570328-developer-apple.com-transform-scale.css",
        },
      ],
    },
  },
  {
    id: "bug1575000",
    platform: "all",
    domain: "apply.lloydsbank.co.uk",
    bug: "1575000",
    contentScripts: {
      matches: ["*://apply.lloydsbank.co.uk/*"],
      css: [
        {
          file:
            "injections/css/bug1575000-apply.lloydsbank.co.uk-radio-buttons-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1605611",
    platform: "android",
    domain: "maps.google.com",
    bug: "1605611",
    contentScripts: {
      matches: InterventionHelpers.matchPatternsForGoogle(
        "*://www.google.",
        "/maps*"
      ),
      css: [
        {
          file: "injections/css/bug1605611-maps.google.com-directions-time.css",
        },
      ],
      js: [
        {
          file: "injections/js/bug1605611-maps.google.com-directions-time.js",
        },
      ],
    },
  },
  {
    id: "bug1610358",
    platform: "android",
    domain: "pcloud.com",
    bug: "1610358",
    contentScripts: {
      matches: ["https://www.pcloud.com/*"],
      js: [
        {
          file: "injections/js/bug1610358-pcloud.com-appVersion-change.js",
        },
      ],
    },
  },
  {
    id: "bug1610344",
    platform: "all",
    domain: "directv.com.co",
    bug: "1610344",
    contentScripts: {
      matches: ["https://*.directv.com.co/*"],
      css: [
        {
          file:
            "injections/css/bug1610344-directv.com.co-hide-unsupported-message.css",
        },
      ],
    },
  },
  {
    id: "bug1644830",
    platform: "desktop",
    domain: "usps.com",
    bug: "1644830",
    contentScripts: {
      matches: ["https://*.usps.com/*"],
      css: [
        {
          file:
            "injections/css/bug1644830-missingmail.usps.com-checkboxes-not-visible.css",
        },
      ],
    },
  },
  {
    id: "bug1651917",
    platform: "android",
    domain: "teletrader.com",
    bug: "1651917",
    contentScripts: {
      matches: ["*://*.teletrader.com/*"],
      css: [
        {
          file:
            "injections/css/bug1651917-teletrader.com.body-transform-origin.css",
        },
      ],
    },
  },
  {
    id: "bug1653075",
    platform: "desktop",
    domain: "livescience.com",
    bug: "1653075",
    contentScripts: {
      matches: ["*://*.livescience.com/*"],
      css: [
        {
          file: "injections/css/bug1653075-livescience.com-scrollbar-width.css",
        },
      ],
    },
  },
  {
    id: "bug1654877",
    platform: "android",
    domain: "preev.com",
    bug: "1654877",
    contentScripts: {
      matches: ["*://preev.com/*"],
      css: [
        {
          file: "injections/css/bug1654877-preev.com-moz-appearance-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1654907",
    platform: "android",
    domain: "reactine.ca",
    bug: "1654907",
    contentScripts: {
      matches: ["*://*.reactine.ca/*"],
      css: [
        {
          file: "injections/css/bug1654907-reactine.ca-hide-unsupported.css",
        },
      ],
    },
  },
  {
    id: "bug1631811",
    platform: "all",
    domain: "datastudio.google.com",
    bug: "1631811",
    contentScripts: {
      matches: ["https://datastudio.google.com/embed/reporting/*"],
      js: [
        {
          file: "injections/js/bug1631811-datastudio.google.com-indexedDB.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1694470",
    platform: "android",
    domain: "m.myvidster.com",
    bug: "1694470",
    contentScripts: {
      matches: ["https://m.myvidster.com/*"],
      css: [
        {
          file: "injections/css/bug1694470-myvidster.com-content-not-shown.css",
        },
      ],
    },
  },
  {
    id: "bug1731825",
    platform: "desktop",
    domain: "Office 365 email handling prompt",
    bug: "1731825",
    contentScripts: {
      matches: [
        "*://*.live.com/*",
        "*://*.office.com/*",
        "*://*.sharepoint.com/*",
        "*://*.office365.com/*",
      ],
      js: [
        {
          file:
            "injections/js/bug1731825-office365-email-handling-prompt-autohide.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1707795",
    platform: "desktop",
    domain: "Office Excel spreadsheets",
    bug: "1707795",
    contentScripts: {
      matches: [
        "*://*.live.com/*",
        "*://*.office.com/*",
        "*://*.sharepoint.com/*",
      ],
      css: [
        {
          file:
            "injections/css/bug1707795-office365-sheets-overscroll-disable.css",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1712833",
    platform: "all",
    domain: "buskocchi.desuca.co.jp",
    bug: "1712833",
    contentScripts: {
      matches: ["*://buskocchi.desuca.co.jp/*"],
      css: [
        {
          file:
            "injections/css/bug1712833-buskocchi.desuca.co.jp-fix-map-height.css",
        },
      ],
    },
  },
  {
    id: "bug1722955",
    platform: "android",
    domain: "frontgate.com",
    bug: "1722955",
    contentScripts: {
      matches: ["*://*.frontgate.com/*"],
      js: [
        {
          file: "lib/ua_helpers.js",
        },
        {
          file: "injections/js/bug1722955-frontgate.com-ua-override.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1724764",
    platform: "android",
    domain: "amextravel.com",
    bug: "1724764",
    contentScripts: {
      matches: ["*://*.amextravel.com/*"],
      js: [
        {
          file: "injections/js/bug1724764-amextravel.com-window-print.js",
        },
      ],
    },
  },
  {
    id: "bug1724868",
    platform: "android",
    domain: "news.yahoo.co.jp",
    bug: "1724868",
    contentScripts: {
      matches: ["*://news.yahoo.co.jp/articles/*", "*://s.yimg.jp/*"],
      js: [
        {
          file: "injections/js/bug1724868-news.yahoo.co.jp-ua-override.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1741234",
    platform: "all",
    domain: "patient.alphalabs.ca",
    bug: "1741234",
    contentScripts: {
      matches: ["*://patient.alphalabs.ca/*"],
      css: [
        {
          file: "injections/css/bug1741234-patient.alphalabs.ca-height-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1743614",
    platform: "android",
    domain: "storytel.com",
    bug: "1743614",
    contentScripts: {
      matches: ["*://*.storytel.com/*"],
      css: [
        {
          file: "injections/css/bug1743614-storytel.com-flex-min-width.css",
        },
      ],
    },
  },
  {
    id: "bug1751022",
    platform: "android",
    domain: "chotot.com",
    bug: "1751022",
    contentScripts: {
      matches: ["*://*.chotot.com/*"],
      css: [
        {
          file: "injections/css/bug1751022-chotot.com-image-width-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1754473",
    platform: "android",
    domain: "m.intl.taobao.com",
    bug: "1754473",
    contentScripts: {
      matches: ["*://m.intl.taobao.com/*"],
      css: [
        {
          file:
            "injections/css/bug1754473-m.intl.taobao.com-number-arrow-buttons-overlapping-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1748455",
    platform: "android",
    domain: "reddit.com",
    bug: "1748455",
    contentScripts: {
      matches: ["*://*.reddit.com/*"],
      css: [
        {
          file:
            "injections/css/bug1748455-reddit.com-gallery-image-width-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1739489",
    platform: "desktop",
    domain: "Sites using draft.js",
    bug: "1739489",
    contentScripts: {
      matches: [
        "*://draftjs.org/*", // Bug 1739489
        "*://www.facebook.com/*", // Bug 1739489
        "*://twitter.com/*", // Bug 1776229
        "*://mobile.twitter.com/*", // Bug 1776229
      ],
      js: [
        {
          file: "injections/js/bug1739489-draftjs-beforeinput.js",
        },
      ],
    },
  },
  {
    id: "bug1765947",
    platform: "android",
    domain: "veniceincoming.com",
    bug: "1765947",
    contentScripts: {
      matches: ["*://veniceincoming.com/*"],
      css: [
        {
          file: "injections/css/bug1765947-veniceincoming.com-left-fix.css",
        },
      ],
    },
  },
  {
    id: "bug11769762",
    platform: "all",
    domain: "tiktok.com",
    bug: "1769762",
    contentScripts: {
      matches: ["https://www.tiktok.com/*"],
      js: [
        {
          file: "injections/js/bug1769762-tiktok.com-plugins-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1770962",
    platform: "all",
    domain: "coldwellbankerhomes.com",
    bug: "1770962",
    contentScripts: {
      matches: ["*://*.coldwellbankerhomes.com/*"],
      css: [
        {
          file:
            "injections/css/bug1770962-coldwellbankerhomes.com-image-height.css",
        },
      ],
    },
  },
  {
    id: "bug1774490",
    platform: "all",
    domain: "rainews.it",
    bug: "1774490",
    contentScripts: {
      matches: ["*://www.rainews.it/*"],
      css: [
        {
          file: "injections/css/bug1774490-rainews.it-gallery-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1772949",
    platform: "all",
    domain: "YouTube embeds",
    bug: "1772949",
    customFunc: "runScriptBeforeRequest",
    script: "injections/js/bug1772949-youtube-webshare-shim.js",
    request: ["*://www.youtube.com/*/www-embed-player.js*"],
    message: "The WebShare API is being disabled on a YouTube frame.",
  },
  {
    id: "bug1778239",
    platform: "all",
    domain: "m.pji.co.kr",
    bug: "1778239",
    contentScripts: {
      matches: ["*://m.pji.co.kr/*"],
      js: [
        {
          file: "injections/js/bug1778239-m.pji.co.kr-banner-hide.js",
        },
      ],
    },
  },
  {
    id: "bug1774005",
    platform: "all",
    domain: "Sites relying on window.InstallTrigger",
    bug: "1774005",
    contentScripts: {
      matches: [
        "*://*.crunchyroll.com/*", // Bug 1777597
        "*://*.pixiv.net/*", // Bug 1774006
        "*://*.webex.com/*", // Bug 1788934
        "*://business.help.royalmail.com/app/webforms/*", // Bug 1786404
        "*://www.northcountrypublicradio.org/contact/subscribe.html*", // Bug 1778382
      ],
      js: [
        {
          file: "injections/js/bug1774005-installtrigger-shim.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1784302",
    platform: "android",
    domain: "open.toutiao.com",
    bug: "1784302",
    contentScripts: {
      matches: ["*://open.toutiao.com/*"],
      js: [
        {
          file: "injections/js/bug1784302-effectiveType-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1784309",
    platform: "all",
    domain: "bet365.com",
    bug: "1784309",
    contentScripts: {
      matches: [
        "*://*.bet365.com/*",
        "*://*.bet365.gr/*",
        "*://*.bet365.com.au/*",
        "*://*.bet365.de/*",
        "*://*.bet365.es/*",
        "*://*.bet365.ca/*",
        "*://*.bet365.dk/*",
        "*://*.bet365.mx/*",
        "*://*.bet365.bet.ar/*",
      ],
      js: [
        {
          file: "injections/js/bug1784309-bet365.com-math-pow.js",
        },
      ],
    },
  },
  {
    id: "bug1784141",
    platform: "android",
    domain: "aveeno.com",
    bug: "1784141",
    contentScripts: {
      matches: [
        "*://*.aveeno.com/*",
        "*://*.aveeno.ca/*",
        "*://*.aveeno.com.au/*",
        "*://*.aveeno.co.kr/*",
        "*://*.aveeno.co.uk/*",
        "*://*.aveeno.ie/*",
      ],
      css: [
        {
          file: "injections/css/bug1784141-aveeno.com-unsupported.css",
        },
      ],
    },
  },
  {
    id: "bug1784195",
    platform: "android",
    domain: "nutmeg.morrisons.com",
    bug: "1784195",
    contentScripts: {
      matches: ["*://nutmeg.morrisons.com/*"],
      css: [
        {
          file: "injections/css/bug1784195-nutmeg.morrisons.com-overflow.css",
        },
      ],
    },
  },
  {
    id: "bug1784546",
    platform: "android",
    domain: "seznam.cz",
    bug: "1784546",
    contentScripts: {
      matches: ["*://*.seznam.cz/*"],
      css: [
        {
          file: "injections/css/bug1784546-seznam.cz-popup-height.css",
        },
      ],
    },
  },
  {
    id: "bug1784351",
    platform: "desktop",
    domain: "movistar.com.ar",
    bug: "1784351",
    contentScripts: {
      matches: ["*://*.movistar.com.ar/*"],
      css: [
        {
          file:
            "injections/css/bug1784351-movistar.com.ar-overflow-overlay-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1784199",
    platform: "all",
    domain: "Sites based on Entrata Platform",
    bug: "1784199",
    contentScripts: {
      matches: [
        "*://*.aptsovation.com/*",
        "*://*.nhcalaska.com/*",
        "*://*.securityproperties.com/*",
        "*://*.theloftsorlando.com/*",
      ],
      css: [
        {
          file: "injections/css/bug1784199-entrata-platform-unsupported.css",
        },
      ],
    },
  },
  {
    id: "bug1787267",
    platform: "all",
    domain: "All international Nintendo domains",
    bug: "1787267",
    contentScripts: {
      matches: [
        "*://*.mojenintendo.cz/*",
        "*://*.nintendo-europe.com/*",
        "*://*.nintendo.at/*",
        "*://*.nintendo.be/*",
        "*://*.nintendo.ch/*",
        "*://*.nintendo.co.il/*",
        "*://*.nintendo.co.jp/*",
        "*://*.nintendo.co.kr/*",
        "*://*.nintendo.co.nz/*",
        "*://*.nintendo.co.uk/*",
        "*://*.nintendo.co.za/*",
        "*://*.nintendo.com.au/*",
        "*://*.nintendo.com.hk/*",
        "*://*.nintendo.com/*",
        "*://*.nintendo.de/*",
        "*://*.nintendo.dk/*",
        "*://*.nintendo.es/*",
        "*://*.nintendo.fi/*",
        "*://*.nintendo.fr/*",
        "*://*.nintendo.gr/*",
        "*://*.nintendo.hu/*",
        "*://*.nintendo.it/*",
        "*://*.nintendo.nl/*",
        "*://*.nintendo.no/*",
        "*://*.nintendo.pt/*",
        "*://*.nintendo.ru/*",
        "*://*.nintendo.se/*",
        "*://*.nintendo.sk/*",
        "*://*.nintendo.tw/*",
        "*://*.nintendoswitch.com.cn/*",
      ],
      js: [
        {
          file:
            "injections/js/bug1787267-nintendo-window-OnetrustActiveGroups.js",
        },
      ],
    },
  },
  {
    id: "bug1788685",
    platform: "all",
    domain: "microsoftedgetips.microsoft.com",
    bug: "1788685",
    contentScripts: {
      matches: ["*://microsoftedgetips.microsoft.com/*"],
      css: [
        {
          file:
            "injections/css/bug1788685-microsoftedgetips.microsoft.com-gallery-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1789164",
    platform: "all",
    domain: "zdnet.com",
    bug: "1789164",
    contentScripts: {
      matches: ["*://www.zdnet.com/*"],
      css: [
        {
          file: "injections/css/bug1789164-zdnet.com-cropped-section.css",
        },
      ],
    },
  },
];

module.exports = AVAILABLE_INJECTIONS;
