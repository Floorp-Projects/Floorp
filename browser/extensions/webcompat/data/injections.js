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
    id: "bug1570856",
    platform: "android",
    domain: "medium.com",
    bug: "1570856",
    contentScripts: {
      matches: ["*://medium.com/*"],
      js: [
        {
          file: "injections/js/bug1570856-medium.com-menu-isTier1.js",
        },
      ],
      allFrames: true,
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
    id: "bug1623375",
    platform: "android",
    domain: "Salesforce communities",
    bug: "1623375",
    contentScripts: {
      matches: [].concat(
        [
          "https://faq.usps.com/*",
          "https://help.duo.com/*",
          "https://my211.force.com/*",
          "https://support.paypay.ne.jp/*",
          "https://usps.force.com/*",
          "https://help.twitch.tv/*",
          "https://support.sonos.com/*",
          "https://us.community.sony.com/*",
          "https://help.shopee.ph/*",
          "https://exclusions.ustr.gov/*",
          "https://help.doordash.com/*",
          "https://community.snowflake.com/*",
          "https://tivoidp.tivo.com/*",
        ],
        InterventionHelpers.matchPatternsForTLDs(
          "*://support.ancestry.",
          "/*",
          ["ca", "co.uk", "com", "com.au", "de", "fr", "it", "mx", "se"]
        )
      ),
      js: [
        {
          file:
            "injections/js/bug1623375-salesforce-communities-hide-unsupported.js",
        },
      ],
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
    id: "bug1577870",
    platform: "desktop",
    domain: "Download prompt for files with no content-type",
    bug: "1577870",
    data: {
      urls: [
        "https://ads-us.rd.linksynergy.com/as.php*",
        "https://www.office.com/logout?sid*",
      ],
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
    id: "bug1567610",
    platform: "all",
    domain: "dns.google.com",
    bug: "1567610",
    contentScripts: {
      matches: ["*://dns.google.com/*", "*://dns.google/*"],
      css: [
        {
          file: "injections/css/bug1567610-dns.google.com-moz-fit-content.css",
        },
      ],
    },
  },
  {
    id: "bug1568256",
    platform: "android",
    domain: "zertifikate.commerzbank.de",
    bug: "1568256",
    contentScripts: {
      matches: ["*://*.zertifikate.commerzbank.de/webforms/mobile/*"],
      css: [
        {
          file: "injections/css/bug1568256-zertifikate.commerzbank.de-flex.css",
        },
      ],
    },
  },
  {
    id: "bug1568908",
    platform: "desktop",
    domain: "console.cloud.google.com",
    bug: "1568908",
    contentScripts: {
      matches: ["*://*.console.cloud.google.com/*"],
      css: [
        {
          file:
            "injections/css/bug1568908-console.cloud.google.com-scrollbar-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1570119",
    platform: "desktop",
    domain: "teamcoco.com",
    bug: "1570119",
    contentScripts: {
      matches: ["*://teamcoco.com/*"],
      css: [
        {
          file: "injections/css/bug1570119-teamcoco.com-scrollbar-width.css",
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
    id: "bug1577270",
    platform: "android",
    domain: "binance.com",
    bug: "1577270",
    contentScripts: {
      matches: ["*://*.binance.com/*"],
      css: [
        {
          file: "injections/css/bug1577270-binance.com-calc-height-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1577297",
    platform: "android",
    domain: "kitkat.com.au",
    bug: "1577297",
    contentScripts: {
      matches: ["*://*.kitkat.com.au/*"],
      css: [
        {
          file: "injections/css/bug1577297-kitkat.com.au-slider-width-fix.css",
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
    id: "bug1609991",
    platform: "android",
    domain: "www.cracked.com",
    bug: "1609991",
    contentScripts: {
      matches: ["https://www.cracked.com/*"],
      css: [
        {
          file: "injections/css/bug1609991-cracked.com-flex-basis-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1610016",
    platform: "android",
    domain: "gaana.com",
    bug: "1610016",
    contentScripts: {
      matches: ["https://gaana.com/*"],
      css: [
        {
          file: "injections/css/bug1610016-gaana.com-input-position-fix.css",
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
    id: "bug1622062",
    platform: "android",
    domain: "$.detectSwipe fix",
    bug: "1622062",
    data: {
      urls: ["https://eu.stemwijzer.nl/public/js/votematch.vendors.js"],
      types: ["script"],
    },
    customFunc: "detectSwipeFix",
  },
  {
    id: "bug1625224",
    platform: "all",
    domain: "sixt-neuwagen.de",
    bug: "1625224",
    contentScripts: {
      matches: ["*://*.sixt-neuwagen.de/*"],
      js: [
        {
          file:
            "injections/js/bug1625224-sixt-neuwagen.de-window-netscape-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1631960",
    platform: "all",
    domain: "websube.ckbogazici.com.tr",
    bug: "1631960",
    contentScripts: {
      matches: ["https://websube.ckbogazici.com.tr/*"],
      css: [
        {
          file:
            "injections/css/bug1631960-websube.ckbogazici.com.tr-table-row-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1632019",
    platform: "all",
    domain: "everyman.co",
    bug: "1632019",
    contentScripts: {
      matches: ["https://everyman.co/*"],
      css: [
        {
          file: "injections/css/bug1632019-everyman.co-gallery-width-fix.css",
        },
      ],
    },
  },
];

module.exports = AVAILABLE_INJECTIONS;
