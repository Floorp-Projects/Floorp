/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["URLSearchParams"]);

const kExtensionID = "followonsearch@mozilla.com";
const kSaveTelemetryMsg = `${kExtensionID}:save-telemetry`;
const kShutdownMsg = `${kExtensionID}:shutdown`;
const kLastSearchQueueDepth = 10;

/**
 * A map of search domains with their expected codes.
 */
let searchDomains = [{
  "domains": [ "search.yahoo.co.jp" ],
  "search": "p",
  "followOnSearch": "ai",
  "prefix": ["fr"],
  "codes": ["mozff"],
  "sap": "yahoo",
}, {
  "domains": [ "www.bing.com" ],
  "search": "q",
  "prefix": ["pc"],
  "reportPrefix": "form",
  "codes": ["MOZI", "MOZD", "MZSL01", "MZSL02", "MZSL03", "MOZ2"],
  "sap": "bing",
}, {
  // The Yahoo domains to watch for.
  "domains": [
    "search.yahoo.com", "ca.search.yahoo.com", "hk.search.yahoo.com",
    "tw.search.yahoo.com", "mozilla.search.yahoo.com", "us.search.yahoo.com",
    "no.search.yahoo.com", "ar.search.yahoo.com", "br.search.yahoo.com",
    "ch.search.yahoo.com", "cl.search.yahoo.com", "de.search.yahoo.com",
    "uk.search.yahoo.com", "es.search.yahoo.com", "espanol.search.yahoo.com",
    "fi.search.yahoo.com", "fr.search.yahoo.com", "nl.search.yahoo.com",
    "id.search.yahoo.com", "in.search.yahoo.com", "it.search.yahoo.com",
    "mx.search.yahoo.com", "se.search.yahoo.com", "sg.search.yahoo.com",
  ],
  "search": "p",
  "followOnSearch": "fr2",
  "prefix": ["hspart", "fr"],
  "reportPrefix": "hsimp",
  "codes": ["mozilla", "moz35"],
  "sap": "yahoo",
}, {
  // The Google domains.
  "domains": [
    "www.google.com", "www.google.ac", "www.google.ad", "www.google.ae",
    "www.google.com.af", "www.google.com.ag", "www.google.com.ai",
    "www.google.al", "www.google.am", "www.google.co.ao", "www.google.com.ar",
    "www.google.as", "www.google.at", "www.google.com.au", "www.google.az",
    "www.google.ba", "www.google.com.bd", "www.google.be", "www.google.bf",
    "www.google.bg", "www.google.com.bh", "www.google.bi", "www.google.bj",
    "www.google.com.bn", "www.google.com.bo", "www.google.com.br",
    "www.google.bs", "www.google.bt", "www.google.co.bw", "www.google.by",
    "www.google.com.bz", "www.google.ca", "www.google.com.kh", "www.google.cc",
    "www.google.cd", "www.google.cf", "www.google.cat", "www.google.cg",
    "www.google.ch", "www.google.ci", "www.google.co.ck", "www.google.cl",
    "www.google.cm", "www.google.cn", "www.google.com.co", "www.google.co.cr",
    "www.google.com.cu", "www.google.cv", "www.google.cx", "www.google.com.cy",
    "www.google.cz", "www.google.de", "www.google.dj", "www.google.dk",
    "www.google.dm", "www.google.com.do", "www.google.dz", "www.google.com.ec",
    "www.google.ee", "www.google.com.eg", "www.google.es", "www.google.com.et",
    "www.google.eu", "www.google.fi", "www.google.com.fj", "www.google.fm",
    "www.google.fr", "www.google.ga", "www.google.ge", "www.google.gf",
    "www.google.gg", "www.google.com.gh", "www.google.com.gi", "www.google.gl",
    "www.google.gm", "www.google.gp", "www.google.gr", "www.google.com.gt",
    "www.google.gy", "www.google.com.hk", "www.google.hn", "www.google.hr",
    "www.google.ht", "www.google.hu", "www.google.co.id", "www.google.iq",
    "www.google.ie", "www.google.co.il", "www.google.im", "www.google.co.in",
    "www.google.io", "www.google.is", "www.google.it", "www.google.je",
    "www.google.com.jm", "www.google.jo", "www.google.co.jp", "www.google.co.ke",
    "www.google.ki", "www.google.kg", "www.google.co.kr", "www.google.com.kw",
    "www.google.kz", "www.google.la", "www.google.com.lb", "www.google.com.lc",
    "www.google.li", "www.google.lk", "www.google.co.ls", "www.google.lt",
    "www.google.lu", "www.google.lv", "www.google.com.ly", "www.google.co.ma",
    "www.google.md", "www.google.me", "www.google.mg", "www.google.mk",
    "www.google.ml", "www.google.com.mm", "www.google.mn", "www.google.ms",
    "www.google.com.mt", "www.google.mu", "www.google.mv", "www.google.mw",
    "www.google.com.mx", "www.google.com.my", "www.google.co.mz",
    "www.google.com.na", "www.google.ne", "www.google.nf", "www.google.com.ng",
    "www.google.com.ni", "www.google.nl", "www.google.no", "www.google.com.np",
    "www.google.nr", "www.google.nu", "www.google.co.nz", "www.google.com.om",
    "www.google.com.pk", "www.google.com.pa", "www.google.com.pe",
    "www.google.com.ph", "www.google.pl", "www.google.com.pg", "www.google.pn",
    "www.google.com.pr", "www.google.ps", "www.google.pt", "www.google.com.py",
    "www.google.com.qa", "www.google.ro", "www.google.rs", "www.google.ru",
    "www.google.rw", "www.google.com.sa", "www.google.com.sb", "www.google.sc",
    "www.google.se", "www.google.com.sg", "www.google.sh", "www.google.si",
    "www.google.sk", "www.google.com.sl", "www.google.sn", "www.google.sm",
    "www.google.so", "www.google.st", "www.google.sr", "www.google.com.sv",
    "www.google.td", "www.google.tg", "www.google.co.th", "www.google.com.tj",
    "www.google.tk", "www.google.tl", "www.google.tm", "www.google.to",
    "www.google.tn", "www.google.com.tr", "www.google.tt", "www.google.com.tw",
    "www.google.co.tz", "www.google.com.ua", "www.google.co.ug",
    "www.google.co.uk", "www.google.us", "www.google.com.uy", "www.google.co.uz",
    "www.google.com.vc", "www.google.co.ve", "www.google.vg", "www.google.co.vi",
    "www.google.com.vn", "www.google.vu", "www.google.ws", "www.google.co.za",
    "www.google.co.zm", "www.google.co.zw",
  ],
  "search": "q",
  "prefix": ["client"],
  "followOnSearch": "oq",
  "codes": ["firefox-b-ab", "firefox-b"],
  "sap": "google",
}];

function getSearchDomainCodes(host) {
  for (let domainInfo of searchDomains) {
    if (domainInfo.domains.includes(host)) {
      return domainInfo;
    }
  }
  return null;
}

/**
 * Used for debugging to log messages.
 *
 * @param {String} message The message to log.
 */
function log(message) {
  // console.log(message);
}

// Hack to handle the most common reload/back/forward case.
// If gLastSearchQueue includes the current URL, ignore the search.
// This also prevents us from handling reloads with hashes twice
let gLastSearchQueue = [];
gLastSearchQueue.push = function(...args) {
  if (this.length >= kLastSearchQueueDepth) {
    this.shift();
  }
  return Array.prototype.push.apply(this, args);
};

// Track if we are in the middle of a Google session
// that started from Firefox
let searchingGoogle = false;

/**
 * Since most codes are in the URL, we can handle them via
 * a progress listener.
 */
var webProgressListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener, Ci.nsISupportsWeakReference]),
  onLocationChange(aWebProgress, aRequest, aLocation, aFlags)
  {
    if (aWebProgress.DOMWindow && (aWebProgress.DOMWindow != content)) {
      return;
    }
    try {
      if (!aWebProgress.isTopLevel ||
          // Not a URL
          (!aLocation.schemeIs("http") && !aLocation.schemeIs("https")) ||
          // Doesn't have a query string or a ref
          (!aLocation.query && !aLocation.ref)) {
        searchingGoogle = false;
        return;
      }
      if (gLastSearchQueue.includes(aLocation.spec)) {
        // If it's a recent search, just return. We
        // don't reset searchingGoogle though because
        // we might still be doing that.
        return;
      }
      let domainInfo = getSearchDomainCodes(aLocation.host);
      if (!domainInfo) {
        searchingGoogle = false;
        return;
      }

      let queries = new URLSearchParams(aLocation.query);
      // Yahoo has switched to Unified search so we can get
      // different codes on the same domain. Hack for now
      // to allow two different prefixes for codes
      let code = queries.get(domainInfo.prefix[0]);
      if (!code && domainInfo.prefix.length > 1) {
        code = queries.get(domainInfo.prefix[1]);
      }
      // Special case Google so we can track searches
      // without codes from the browser.
      if (domainInfo.sap == "google") {
        if (aLocation.filePath == "/search") {
          gLastSearchQueue.push(aLocation.spec);
          // Our engine currently sends oe and ie - no one else does
          if (queries.get("oe") && queries.get("ie")) {
            sendSaveTelemetryMsg(code ? code : "none", code ? domainInfo.sap : "google-nocodes", "sap");
            searchingGoogle = true;
          } else {
            // The tbm value is the specific type of search (Books, Images, News, etc).
            // These are referred to as vertical searches.
            let tbm = queries.get("tbm");
            if (searchingGoogle) {
              sendSaveTelemetryMsg(code ? code : "none", code ? domainInfo.sap : "google-nocodes", "follow-on", tbm ? `vertical-${tbm}` : null);
            } else if (code) {
              // Trying to do the right thing for back button to existing entries
              sendSaveTelemetryMsg(code, domainInfo.sap, "follow-on", tbm ? `vertical-${tbm}` : null);
            }
          }
        }
        // Special case all Google. Otherwise our code can
        // show up in maps
        return;
      }
      searchingGoogle = false;
      if (queries.get(domainInfo.search)) {
        if (domainInfo.codes.includes(code)) {
          if (domainInfo.reportPrefix &&
              queries.get(domainInfo.reportPrefix)) {
            code = queries.get(domainInfo.reportPrefix);
          }
          if (queries.get(domainInfo.followOnSearch)) {
            log(`${aLocation.host} search with code ${code} - Follow on`);
            sendSaveTelemetryMsg(code, domainInfo.sap, "follow-on");
          } else {
            log(`${aLocation.host} search with code ${code} - First search via Firefox`);
            sendSaveTelemetryMsg(code, domainInfo.sap, "sap");
          }
          gLastSearchQueue.push(aLocation.spec);
        }
      }
    } catch (e) {
      console.error(e);
    }
  },
};

/**
 * Parses a cookie string into separate parts.
 *
 * @param {String} cookieString The string to parse.
 * @param {Object} [params] An optional object to append the parameters to.
 * @return {Object} An object containing the query keys and values.
 */
function parseCookies(cookieString, params = {}) {
  var cookies = cookieString.split(/;\s*/);

  for (var i in cookies) {
    var kvp = cookies[i].split(/=(.+)/);
    params[kvp[0]] = kvp[1];
  }

  return params;
}

/**
 * Page load listener to handle loads www.bing.com only.
 * We have to use a page load listener because we need
 * to check cookies.
 * @param {Object} event The page load event.
 */
function onPageLoad(event) {
  var doc = event.target;
  var win = doc.defaultView;
  if (win != win.top) {
    return;
  }
  var uri = doc.documentURIObject;
  if (!(uri instanceof Ci.nsIStandardURL) ||
      (!uri.schemeIs("http") && !uri.schemeIs("https")) ||
       uri.host != "www.bing.com" ||
      !doc.location.search ||
      gLastSearchQueue.includes(uri.spec)) {
    return;
  }
  var queries = new URLSearchParams(doc.location.search.toLowerCase());
  // For Bing, QBRE form code is used for all follow-on search
  if (queries.get("form") != "qbre") {
    return;
  }
  if (parseCookies(doc.cookie).SRCHS == "PC=MOZI") {
    log(`${uri.host} search with code MOZI - Follow on`);
    sendSaveTelemetryMsg("MOZI", "bing", "follow-on");
    gLastSearchQueue.push(uri.spec);
  }
}

/**
 * Sends a message to the process that added this script to tell it to save
 * telemetry.
 *
 * @param {String} code The codes used for the search engine.
 * @param {String} sap The SAP code.
 * @param {String} type The type of search (sap/follow-on).
 * @param {String} extra Any additional parameters (Optional)
 */
function sendSaveTelemetryMsg(code, sap, type, extra) {
  sendAsyncMessage(kSaveTelemetryMsg, {
    code,
    sap,
    type,
    extra,
  });
}

addEventListener("DOMContentLoaded", onPageLoad, false);
docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebProgress)
        .addProgressListener(webProgressListener, Ci.nsIWebProgress.NOTIFY_LOCATION);

let gDisabled = false;

addMessageListener(kShutdownMsg, () => {
  if (!gDisabled) {
    removeEventListener("DOMContentLoaded", onPageLoad, false);
    docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebProgress)
            .removeProgressListener(webProgressListener);
    gDisabled = true;
  }
});
