/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyGetter(this, "History", () => {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils.history;
});

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
const {
  normalizeTime,
  SingletonEventManager,
} = ExtensionUtils;

let historySvc = Ci.nsINavHistoryService;
const TRANSITION_TO_TRANSITION_TYPES_MAP = new Map([
  ["link", historySvc.TRANSITION_LINK],
  ["typed", historySvc.TRANSITION_TYPED],
  ["auto_bookmark", historySvc.TRANSITION_BOOKMARK],
  ["auto_subframe", historySvc.TRANSITION_EMBED],
  ["manual_subframe", historySvc.TRANSITION_FRAMED_LINK],
]);

function getTransitionType(transition) {
  // cannot set a default value for the transition argument as the framework sets it to null
  transition = transition || "link";
  let transitionType = TRANSITION_TO_TRANSITION_TYPES_MAP.get(transition);
  if (!transitionType) {
    throw new Error(`|${transition}| is not a supported transition for history`);
  }
  return transitionType;
}

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
    lastVisitTime: PlacesUtils.toDate(node.time).getTime(),
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

var _observer;

function getObserver() {
  if (!_observer) {
    _observer = {
      onDeleteURI: function(uri, guid, reason) {
        this.emit("visitRemoved", {allHistory: false, urls: [uri.spec]});
      },
      onVisit: function() {},
      onBeginUpdateBatch: function() {},
      onEndUpdateBatch: function() {},
      onTitleChanged: function() {},
      onClearHistory: function() {
        this.emit("visitRemoved", {allHistory: true});
      },
      onPageChanged: function() {},
      onFrecencyChanged: function() {},
      onManyFrecenciesChanged: function() {},
      onDeleteVisits: function(uri, time, guid, reason) {
        this.emit("visitRemoved", {allHistory: false, urls: [uri.spec]});
      },
    };
    EventEmitter.decorate(_observer);
    PlacesUtils.history.addObserver(_observer, false);
  }
  return _observer;
}

extensions.registerSchemaAPI("history", "history", (extension, context) => {
  return {
    history: {
      addUrl: function(details) {
        let transition, date;
        try {
          transition = getTransitionType(details.transition);
        } catch (error) {
          return Promise.reject({message: error.message});
        }
        if (details.visitTime) {
          date = normalizeTime(details.visitTime);
        }
        let pageInfo = {
          title: details.title,
          url: details.url,
          visits: [
            {
              transition,
              date,
            },
          ],
        };
        try {
          return History.insert(pageInfo).then(() => undefined);
        } catch (error) {
          return Promise.reject({message: error.message});
        }
      },
      deleteAll: function() {
        return History.clear();
      },
      deleteRange: function(filter) {
        let newFilter = {
          beginDate: normalizeTime(filter.startTime),
          endDate: normalizeTime(filter.endTime),
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

      onVisitRemoved: new SingletonEventManager(context, "history.onVisitRemoved", fire => {
        let listener = (event, data) => {
          context.runSafe(fire, data);
        };

        getObserver().on("visitRemoved", listener);
        return () => {
          getObserver().off("visitRemoved", listener);
        };
      }).api(),
    },
  };
});
