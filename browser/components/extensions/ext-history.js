/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

XPCOMUtils.defineLazyGetter(this, "History", () => {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils.history;
});

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
const {
  normalizeTime,
} = ExtensionUtils;

/*
 * Converts a nsINavHistoryResultNode into a plain object
 *
 * https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryResultNode
 */
function convertNavHistoryResultNode(node) {
  return {
    id: node.pageGuid,
    url: node.uri,
    title: node.title,
    lastVisitTime: PlacesUtils.toTime(node.time),
    visitCount: node.accessCount,
  };
}

/*
 * Converts a nsINavHistoryContainerResultNode into an array of objects
 *
 * https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryContainerResultNode
 */
function convertNavHistoryContainerResultNode(container) {
  let results = [];
  container.containerOpen = true;
  for (let i = 0; i < container.childCount; i++) {
    let node = container.getChild(i);
    results.push(convertNavHistoryResultNode(node));
  }
  container.containerOpen = false;
  return results;
}

extensions.registerSchemaAPI("history", "history", (extension, context) => {
  return {
    history: {
      deleteAll: function() {
        return History.clear();
      },
      deleteRange: function(filter) {
        let newFilter = {
          beginDate: new Date(filter.startTime),
          endDate: new Date(filter.endTime),
        };
        // History.removeVisitsByFilter returns a boolean, but our API should return nothing
        return History.removeVisitsByFilter(newFilter).then(() => undefined);
      },
      deleteUrl: function(details) {
        let url = details.url;
        // History.remove returns a boolean, but our API should return nothing
        return History.remove(url).then(() => undefined);
      },
      search: function(query) {
        let beginTime = (query.startTime == null) ?
                          PlacesUtils.toPRTime(Date.now() - 24 * 60 * 60 * 1000) :
                          PlacesUtils.toPRTime(normalizeTime(query.startTime));
        let endTime = (query.endTime == null) ?
                        Number.MAX_VALUE :
                        PlacesUtils.toPRTime(normalizeTime(query.endTime));
        if (beginTime > endTime) {
          return Promise.reject({message: "The startTime cannot be after the endTime"});
        }

        let options = History.getNewQueryOptions();
        options.sortingMode = options.SORT_BY_DATE_DESCENDING;
        options.maxResults = query.maxResults || 100;

        let historyQuery = History.getNewQuery();
        historyQuery.searchTerms = query.text;
        historyQuery.beginTime = beginTime;
        historyQuery.endTime = endTime;
        let queryResult = History.executeQuery(historyQuery, options).root;
        let results = convertNavHistoryContainerResultNode(queryResult);
        return Promise.resolve(results);
      },
    },
  };
});
