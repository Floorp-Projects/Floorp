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
    id: "bug1645064",
    platform: "desktop",
    domain: "s-kanava.fi",
    bug: "1645064",
    contentScripts: {
      matches: ["https://www.s-kanava.fi/*"],
      css: [
        {
          file: "injections/css/bug1645064-s-kanava.fi-invisible-charts.css",
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
    id: "bug1655049",
    platform: "android",
    domain: "dev.to",
    bug: "1655049",
    contentScripts: {
      matches: ["*://dev.to/*"],
      css: [
        {
          file: "injections/css/bug1655049-dev.to-unclickable-button-fix.css",
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
    id: "bug1677442",
    platform: "desktop",
    domain: "store.hp.com",
    bug: "1677442",
    contentScripts: {
      matches: ["*://d3nkfb7815bs43.cloudfront.net/*forstore.hp.com*"],
      js: [
        {
          file: "injections/js/bug1677442-store.hp.com-disable-indexeddb.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1690158",
    platform: "desktop",
    domain: "slack.com",
    bug: "1690158",
    contentScripts: {
      matches: ["*://app.slack.com/*"],
      css: [
        {
          file: "injections/css/bug1690158-slack.com-webkit-scrollbar.css",
        },
      ],
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
];

module.exports = AVAILABLE_INJECTIONS;
