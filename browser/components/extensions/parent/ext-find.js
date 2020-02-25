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

// A mapping of top-level ExtFind actors to arrays of results in each subframe.
let findResults = new WeakMap();

function getActorForBrowsingContext(browsingContext) {
  let windowGlobal = browsingContext.currentWindowGlobal;
  return windowGlobal ? windowGlobal.getActor("ExtFind") : null;
}

function getTopLevelActor(browser) {
  return getActorForBrowsingContext(browser.browsingContext);
}

function gatherActors(browsingContext) {
  let list = [];

  let actor = getActorForBrowsingContext(browsingContext);
  if (actor) {
    list.push({ actor, result: null });
  }

  let children = browsingContext.getChildren();
  for (let child of children) {
    list.push(...gatherActors(child));
  }

  return list;
}

function mergeFindResults(params, list) {
  let finalResult = {
    count: 0,
  };

  if (params.includeRangeData) {
    finalResult.rangeData = [];
  }
  if (params.includeRectData) {
    finalResult.rectData = [];
  }

  let currentFramePos = -1;
  for (let item of list) {
    if (item.result.count == 0) {
      continue;
    }

    // The framePos is incremented for each different document that has matches.
    currentFramePos++;

    finalResult.count += item.result.count;
    if (params.includeRangeData && item.result.rangeData) {
      for (let range of item.result.rangeData) {
        range.framePos = currentFramePos;
      }

      finalResult.rangeData.push(...item.result.rangeData);
    }

    if (params.includeRectData && item.result.rectData) {
      finalResult.rectData.push(...item.result.rectData);
    }
  }

  return finalResult;
}

function sendMessageToAllActors(browser, message, params) {
  for (let { actor } of gatherActors(browser.browsingContext)) {
    actor.sendAsyncMessage("ext-Finder:" + message, params);
  }
}

async function getFindResultsForActor(findContext, message, params) {
  findContext.result = await findContext.actor.sendQuery(
    "ext-Finder:" + message,
    params
  );
  return findContext;
}

function queryAllActors(browser, message, params) {
  let promises = [];
  for (let findContext of gatherActors(browser.browsingContext)) {
    promises.push(getFindResultsForActor(findContext, message, params));
  }
  return Promise.all(promises);
}

async function collectFindResults(browser, findResults, params) {
  let results = await queryAllActors(browser, "CollectResults", params);
  findResults.set(getTopLevelActor(browser), results);
  return mergeFindResults(params, results);
}

async function runHighlight(browser, params) {
  let hasResults = false;
  let foundResults = false;
  let list = findResults.get(getTopLevelActor(browser));
  if (!list) {
    return Promise.reject({ message: "no search results to highlight" });
  }

  let highlightPromises = [];

  let index = params.rangeIndex;
  for (let c = 0; c < list.length; c++) {
    if (list[c].result.count) {
      hasResults = true;
    }

    let actor = list[c].actor;
    if (!foundResults && index < list[c].result.count) {
      foundResults = true;
      params.rangeIndex = index;
      highlightPromises.push(
        actor.sendQuery("ext-Finder:HighlightResults", params)
      );
    } else {
      highlightPromises.push(
        actor.sendQuery("ext-Finder:ClearHighlighting", params)
      );
    }

    index -= list[c].result.count;
  }

  let responses = await Promise.all(highlightPromises);
  if (hasResults) {
    if (responses.includes("OutOfRange") || index >= 0) {
      return Promise.reject({ message: "index supplied was out of range" });
    } else if (responses.includes("Success")) {
      return;
    }
  }

  return Promise.reject({ message: "no search results to highlight" });
}

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

  if (message == "HighlightResults") {
    return runHighlight(browser, params);
  } else if (message == "CollectResults") {
    // Remove prior highlights before starting a new find operation.
    findResults.delete(getTopLevelActor(browser));
    return collectFindResults(browser, findResults, params);
  }
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
          sendMessageToAllActors(tab.linkedBrowser, "ClearHighlighting", {});
        },
      },
    };
  }
};
