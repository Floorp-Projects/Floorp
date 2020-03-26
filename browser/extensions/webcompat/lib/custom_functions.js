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
  pdk5fix: injection => {
    const { urls, types } = injection.data;
    const listener = (injection.data.listener = ({ requestId }) => {
      replaceStringInRequest(
        requestId,
        "VideoContextChromeAndroid",
        "VideoContextAndroid"
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
