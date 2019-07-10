/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global tabTracker */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

var { ExtensionError } = ExtensionUtils;

/**
 * runFindOperation
 * Utility for `find` and `highlightResults`.
 *
 * @param {BaseContext} context - context the find operation runs in.
 * @param {object} params - params to pass to message sender.
 * @param {string} message - identifying component of message name.
 *
 * @returns {Promise} a promise that will be resolved or rejected based on the
 *          data received by the message listener.
 */
function runFindOperation(context, params, message) {
  let { tabId } = params;
  let tab = tabId ? tabTracker.getTab(tabId) : tabTracker.activeTab;
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;
  tabId = tabId || tabTracker.getId(tab);
  if (
    !context.privateBrowsingAllowed &&
    PrivateBrowsingUtils.isBrowserPrivate(browser)
  ) {
    return Promise.reject({ message: `Unable to search: ${tabId}` });
  }
  // We disallow find in about: urls.
  if (
    tab.linkedBrowser.contentPrincipal.isSystemPrincipal ||
    (["about", "chrome", "resource"].includes(
      tab.linkedBrowser.currentURI.scheme
    ) &&
      tab.linkedBrowser.currentURI.spec != "about:blank")
  ) {
    return Promise.reject({ message: `Unable to search: ${tabId}` });
  }

  return new Promise((resolve, reject) => {
    mm.addMessageListener(
      `ext-Finder:${message}Finished`,
      function messageListener(message) {
        mm.removeMessageListener(
          `ext-Finder:${message}Finished`,
          messageListener
        );
        switch (message.data) {
          case "Success":
            resolve();
            break;
          case "OutOfRange":
            reject({ message: "index supplied was out of range" });
            break;
          case "NoResults":
            reject({ message: "no search results to highlight" });
            break;
        }
        resolve(message.data);
      }
    );
    mm.sendAsyncMessage(`ext-Finder:${message}`, params);
  });
}

this.find = class extends ExtensionAPI {
  getAPI(context) {
    return {
      find: {
        /**
         * browser.find.find
         * Searches document and its frames for a given queryphrase and stores all found
         * Range objects in an array accessible by other browser.find methods.
         *
         * @param {string} queryphrase - The string to search for.
         * @param {object} params optional - may contain any of the following properties,
         *   all of which are optional:
         *   {number} tabId - Tab to query.  Defaults to the active tab.
         *   {boolean} caseSensitive - Highlight only ranges with case sensitive match.
         *   {boolean} entireWord - Highlight only ranges that match entire word.
         *   {boolean} includeRangeData - Whether to return range data.
         *   {boolean} includeRectData - Whether to return rectangle data.
         *
         * @returns {object} data received by the message listener that includes:
         *   {number} count - number of results found.
         *   {array} rangeData (if opted) - serialized representation of ranges found.
         *   {array} rectData (if opted) - rect data of ranges found.
         */
        find(queryphrase, params) {
          params = params || {};
          params.queryphrase = queryphrase;
          return runFindOperation(context, params, "CollectResults");
        },

        /**
         * browser.find.highlightResults
         * Highlights range(s) found in previous browser.find.find.
         *
         * @param {object} params optional - may contain any of the following properties,
         *   all of which are optional:
         *   {number} rangeIndex - Found range to be highlighted. Default highlights all ranges.
         *   {number} tabId - Tab to highlight.  Defaults to the active tab.
         *   {boolean} noScroll - Don't scroll to highlighted item.
         *
         * @returns {string} - data received by the message listener that may be:
         *   "Success" - Highlighting succeeded.
         *   "OutOfRange" - The index supplied was out of range.
         *   "NoResults" - There were no search results to highlight.
         */
        highlightResults(params) {
          params = params || {};
          return runFindOperation(context, params, "HighlightResults");
        },

        /**
         * browser.find.removeHighlighting
         * Removes all highlighting from previous search.
         *
         * @param {number} tabId optional
         *                 Tab to clear highlighting in.  Defaults to the active tab.
         */
        removeHighlighting(tabId) {
          let tab = tabId ? tabTracker.getTab(tabId) : tabTracker.activeTab;
          if (
            !context.privateBrowsingAllowed &&
            PrivateBrowsingUtils.isBrowserPrivate(tab.linkedBrowser)
          ) {
            throw new ExtensionError(`Invalid tab ID: ${tabId}`);
          }
          tab.linkedBrowser.messageManager.sendAsyncMessage(
            "ext-Finder:clearHighlighting"
          );
        },
      },
    };
  }
};
