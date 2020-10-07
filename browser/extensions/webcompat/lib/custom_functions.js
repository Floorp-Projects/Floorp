/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module, promiseConsoleWarningScript */

function getConsoleLogCallback(tabId) {
  return () => {
    // We don't actually know the site's we're hitting with these custom
    // functions, so let's have the console say "this site".
    promiseConsoleWarningScript("this site").then(script => {
      browser.tabs.executeScript(tabId, script).catch(() => {});
    });
  };
}

const replaceStringInRequest = (requestId, inString, outString, callback) => {
  const filter = browser.webRequest.filterResponseData(requestId);
  const decoder = new TextDecoder("utf-8");
  const encoder = new TextEncoder();
  const RE = new RegExp(inString, "g");
  const carryoverLength = inString.length;
  let carryover = "";
  let doCallback = false;

  filter.ondata = event => {
    const replaced = (
      carryover + decoder.decode(event.data, { stream: true })
    ).replace(RE, outString);
    if (callback && replaced.includes(outString)) {
      doCallback = true;
    }
    filter.write(encoder.encode(replaced.slice(0, -carryoverLength)));
    carryover = replaced.slice(-carryoverLength);
  };

  filter.onstop = event => {
    if (carryover.length) {
      filter.write(encoder.encode(carryover));
    }
    filter.close();
    if (doCallback) {
      callback();
    }
  };
};

const CUSTOM_FUNCTIONS = {
  detectSwipeFix: injection => {
    const { urls, types } = injection.data;
    const listener = (injection.data.listener = ({ requestId, tabId }) => {
      replaceStringInRequest(
        requestId,
        "preventDefault:true",
        "preventDefault:false",
        getConsoleLogCallback(tabId)
      );
      return {};
    });
    browser.webRequest.onBeforeRequest.addListener(listener, { urls, types }, [
      "blocking",
    ]);
  },
  detectSwipeFixDisable: injection => {
    const { listener } = injection.data;
    browser.webRequest.onBeforeRequest.removeListener(listener);
    delete injection.data.listener;
  },

  noSniffFix: injection => {
    const { urls, contentType } = injection.data;
    const listener = (injection.data.listener = e => {
      e.responseHeaders.push(contentType);
      return { responseHeaders: e.responseHeaders };
    });

    browser.webRequest.onHeadersReceived.addListener(listener, { urls }, [
      "blocking",
      "responseHeaders",
    ]);
  },
  noSniffFixDisable: injection => {
    const { listener } = injection.data;
    browser.webRequest.onHeadersReceived.removeListener(listener);
    delete injection.data.listener;
  },

  pdk5fix: injection => {
    const { urls, types } = injection.data;
    const listener = (injection.data.listener = ({ requestId, tabId }) => {
      replaceStringInRequest(
        requestId,
        "VideoContextChromeAndroid",
        "VideoContextAndroid",
        getConsoleLogCallback(tabId)
      );
      return {};
    });
    browser.webRequest.onBeforeRequest.addListener(listener, { urls, types }, [
      "blocking",
    ]);
  },
  pdk5fixDisable: injection => {
    const { listener } = injection.data;
    browser.webRequest.onBeforeRequest.removeListener(listener);
    delete injection.data.listener;
  },
};

module.exports = CUSTOM_FUNCTIONS;
