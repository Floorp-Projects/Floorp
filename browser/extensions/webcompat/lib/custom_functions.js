/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module */

const replaceStringInRequest = (
  requestId,
  inString,
  outString,
  inEncoding = "utf-8"
) => {
  const filter = browser.webRequest.filterResponseData(requestId);
  const decoder = new TextDecoder(inEncoding);
  const encoder = new TextEncoder();
  const RE = new RegExp(inString, "g");
  const carryoverLength = inString.length;
  let carryover = "";

  filter.ondata = event => {
    const replaced = (
      carryover + decoder.decode(event.data, { stream: true })
    ).replace(RE, outString);
    filter.write(encoder.encode(replaced.slice(0, -carryoverLength)));
    carryover = replaced.slice(-carryoverLength);
  };

  filter.onstop = event => {
    if (carryover.length) {
      filter.write(encoder.encode(carryover));
    }
    filter.close();
  };
};

const CUSTOM_FUNCTIONS = {
  acceptLanguageFix: injection => {
    const { urls } = injection.data;
    const re = /^([a-zA-Z]{2,3})-/; // match 2 or 3 letters at the start followed by a hyphen
    const listener = (injection.data.listener = e => {
      for (const header of e.requestHeaders) {
        if (header.name.toLowerCase() === "accept-language") {
          const match = header.value.match(re);
          if (match) {
            // add country code to start with comma -> 'en-US,en;q=0.5' to 'en,en-US,en;q=0.5'
            header.value = `${match[1]},${header.value}`;
          }
        }
      }
      return { requestHeaders: e.requestHeaders };
    });
    browser.webRequest.onBeforeSendHeaders.addListener(listener, { urls }, [
      "blocking",
      "requestHeaders",
    ]);
  },
  acceptLanguageFixDisable: injection => {
    const { listener } = injection.data;
    browser.webRequest.onBeforeSendHeaders.removeListener(listener);
    delete injection.data.listener;
  },
  detectSwipeFix: injection => {
    const { urls, types } = injection.data;
    const listener = (injection.data.listener = ({ requestId }) => {
      replaceStringInRequest(
        requestId,
        "preventDefault:true",
        "preventDefault:false"
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
  runScriptBeforeRequest: injection => {
    const { bug, message, request, script, types } = injection;
    const warning = `${message} See https://bugzilla.mozilla.org/show_bug.cgi?id=${bug} for details.`;

    const listener = (injection.listener = e => {
      const { tabId, frameId } = e;
      return browser.tabs
        .executeScript(tabId, {
          file: script,
          frameId,
          runAt: "document_start",
        })
        .then(() => {
          browser.tabs.executeScript(tabId, {
            code: `console.warn(${JSON.stringify(warning)})`,
            runAt: "document_start",
          });
        })
        .catch(_ => {});
    });

    browser.webRequest.onBeforeRequest.addListener(
      listener,
      { urls: request, types: types || ["script"] },
      ["blocking"]
    );
  },
  runScriptBeforeRequestDisable: injection => {
    const { listener } = injection;
    browser.webRequest.onBeforeRequest.removeListener(listener);
    delete injection.data.listener;
  },
};

module.exports = CUSTOM_FUNCTIONS;
