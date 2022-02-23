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
    id: "bug1561371",
    platform: "android",
    domain: "mail.google.com",
    bug: "1561371",
    contentScripts: {
      matches: ["*://mail.google.com/*"],
      css: [
        {
          file:
            "injections/css/bug1561371-mail.google.com-allow-horizontal-scrolling.css",
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
    id: "bug1666771",
    platform: "desktop",
    domain: "zillow.com",
    bug: "1666771",
    contentScripts: {
      allFrames: true,
      matches: ["*://*.zillow.com/*"],
      css: [
        {
          file: "injections/css/bug1666771-zilow-map-overdraw.css",
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
    id: "bug1704653",
    platform: "all",
    domain: "tsky.in",
    bug: "1704653",
    contentScripts: {
      matches: ["*://tsky.in/*"],
      css: [
        {
          file: "injections/css/bug1704653-tsky.in-clear-float.css",
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
    id: "bug1711082",
    platform: "all",
    domain: "m.aliexpress.com",
    bug: "1711082",
    contentScripts: {
      matches: ["*://m.aliexpress.com/*"],
      js: [
        {
          file: "injections/js/bug1711082-m.aliexpress.com-undisable-search.js",
        },
      ],
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
    id: "bug1719870",
    platform: "all",
    domain: "lcbo.com",
    bug: "1719870",
    contentScripts: {
      matches: ["*://*.lcbo.com/*"],
      css: [
        {
          file: "injections/css/bug1719870-lcbo.com-table-clearfix.css",
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
    id: "bug1727080",
    platform: "android",
    domain: "nexity.fr",
    bug: "1727080",
    contentScripts: {
      matches: ["*://*.nexity.fr/*"],
      css: [
        {
          file: "injections/css/bug1727080-nexity.fr-svg-size-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1738313",
    platform: "desktop",
    domain: "curriculum.gov.bc.ca",
    bug: "1738313",
    contentScripts: {
      matches: ["*://curriculum.gov.bc.ca/*"],
      css: [
        {
          file:
            "injections/css/bug1738313-curriculum.gov.bc.ca-bootstrap-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1738316",
    platform: "android",
    domain: "vuoriclothing.com",
    bug: "1738316",
    contentScripts: {
      matches: ["*://vuoriclothing.com/*"],
      css: [
        {
          file: "injections/css/bug1738316-vuoriclothing.com-flexbox-fix.css",
        },
      ],
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
    id: "bug1740542",
    platform: "desktop",
    domain: "tesla.com",
    bug: "1740542",
    contentScripts: {
      matches: ["*://*.tesla.com/*"],
      css: [
        {
          file: "injections/css/bug1740542-tesla.com-footer-links.css",
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
    id: "bug1749565",
    platform: "android",
    domain: "bonappetit.com",
    bug: "1749565",
    contentScripts: {
      matches: ["*://*.bonappetit.com/recipe/*"],
      css: [
        {
          file: "injections/css/bug1749565-bonappetit.com-grid-width-fix.css",
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
    id: "bug1751065",
    platform: "android",
    domain: "chanel.com",
    bug: "1751065",
    contentScripts: {
      matches: ["*://*.chanel.com/*"],
      js: [
        {
          file: "injections/js/bug1751065-chanel.com-effectiveType-shim.js",
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
];

module.exports = AVAILABLE_INJECTIONS;
