/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

var {
  normalizeTime,
  SingletonEventManager,
} = ExtensionUtils;

let nsINavHistoryService = Ci.nsINavHistoryService;
const TRANSITION_TO_TRANSITION_TYPES_MAP = new Map([
  ["link", nsINavHistoryService.TRANSITION_LINK],
  ["typed", nsINavHistoryService.TRANSITION_TYPED],
  ["auto_bookmark", nsINavHistoryService.TRANSITION_BOOKMARK],
  ["auto_subframe", nsINavHistoryService.TRANSITION_EMBED],
  ["manual_subframe", nsINavHistoryService.TRANSITION_FRAMED_LINK],
]);

let TRANSITION_TYPE_TO_TRANSITIONS_MAP = new Map();
for (let [transition, transitionType] of TRANSITION_TO_TRANSITION_TYPES_MAP) {
  TRANSITION_TYPE_TO_TRANSITIONS_MAP.set(transitionType, transition);
}

function getTransitionType(transition) {
  // cannot set a default value for the transition argument as the framework sets it to null
  transition = transition || "link";
  let transitionType = TRANSITION_TO_TRANSITION_TYPES_MAP.get(transition);
  if (!transitionType) {
    throw new Error(`|${transition}| is not a supported transition for history`);
  }
  return transitionType;
}

function getTransition(transitionType) {
  return TRANSITION_TYPE_TO_TRANSITIONS_MAP.get(transitionType) || "link";
}

/*
 * Converts a nsINavHistoryResultNode into a HistoryItem
 *
 * https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryResultNode
 */
function convertNodeToHistoryItem(node) {
  return {
    id: node.pageGuid,
    url: node.uri,
    title: node.title,
    lastVisitTime: PlacesUtils.toDate(node.time).getTime(),
    visitCount: node.accessCount,
  };
}

/*
 * Converts a nsINavHistoryResultNode into a VisitItem
 *
 * https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryResultNode
 */
function convertNodeToVisitItem(node) {
  return {
    id: node.pageGuid,
    visitId: node.visitId,
    visitTime: PlacesUtils.toDate(node.time).getTime(),
    referringVisitId: node.fromVisitId,
    transition: getTransition(node.visitType),
  };
}

/*
 * Converts a nsINavHistoryContainerResultNode into an array of objects
 *
 * https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsINavHistoryContainerResultNode
 */
function convertNavHistoryContainerResultNode(container, converter) {
  let results = [];
  container.containerOpen = true;
  for (let i = 0; i < container.childCount; i++) {
    let node = container.getChild(i);
    results.push(converter(node));
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
      onVisit: function(uri, visitId, time, sessionId, referringId, transitionType, guid, hidden, visitCount, typed) {
        let data = {
          id: guid,
          url: uri.spec,
          title: "",
          lastVisitTime: time / 1000,  // time from Places is microseconds,
          visitCount,
          typedCount: typed,
        };
        this.emit("visited", data);
      },
      onBeginUpdateBatch: function() {},
      onEndUpdateBatch: function() {},
      onTitleChanged: function(uri, title) {
        this.emit("titleChanged", {url: uri.spec, title: title});
      },
      onClearHistory: function() {
        this.emit("visitRemoved", {allHistory: true, urls: []});
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

this.history = class extends ExtensionAPI {
  getAPI(context) {
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
            return PlacesUtils.history.insert(pageInfo).then(() => undefined);
          } catch (error) {
            return Promise.reject({message: error.message});
          }
        },

        deleteAll: function() {
          return PlacesUtils.history.clear();
        },

        deleteRange: function(filter) {
          let newFilter = {
            beginDate: normalizeTime(filter.startTime),
            endDate: normalizeTime(filter.endTime),
          };
          // History.removeVisitsByFilter returns a boolean, but our API should return nothing
          return PlacesUtils.history.removeVisitsByFilter(newFilter).then(() => undefined);
        },

        deleteUrl: function(details) {
          let url = details.url;
          // History.remove returns a boolean, but our API should return nothing
          return PlacesUtils.history.remove(url).then(() => undefined);
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

          let options = PlacesUtils.history.getNewQueryOptions();
          options.sortingMode = options.SORT_BY_DATE_DESCENDING;
          options.maxResults = query.maxResults || 100;

          let historyQuery = PlacesUtils.history.getNewQuery();
          historyQuery.searchTerms = query.text;
          historyQuery.beginTime = beginTime;
          historyQuery.endTime = endTime;
          let queryResult = PlacesUtils.history.executeQuery(historyQuery, options).root;
          let results = convertNavHistoryContainerResultNode(queryResult, convertNodeToHistoryItem);
          return Promise.resolve(results);
        },

        getVisits: function(details) {
          let url = details.url;
          if (!url) {
            return Promise.reject({message: "A URL must be provided for getVisits"});
          }

          let options = PlacesUtils.history.getNewQueryOptions();
          options.sortingMode = options.SORT_BY_DATE_DESCENDING;
          options.resultType = options.RESULTS_AS_VISIT;

          let historyQuery = PlacesUtils.history.getNewQuery();
          historyQuery.uri = NetUtil.newURI(url);
          let queryResult = PlacesUtils.history.executeQuery(historyQuery, options).root;
          let results = convertNavHistoryContainerResultNode(queryResult, convertNodeToVisitItem);
          return Promise.resolve(results);
        },

        onVisited: new SingletonEventManager(context, "history.onVisited", fire => {
          let listener = (event, data) => {
            fire.sync(data);
          };

          getObserver().on("visited", listener);
          return () => {
            getObserver().off("visited", listener);
          };
        }).api(),

        onVisitRemoved: new SingletonEventManager(context, "history.onVisitRemoved", fire => {
          let listener = (event, data) => {
            fire.sync(data);
          };

          getObserver().on("visitRemoved", listener);
          return () => {
            getObserver().off("visitRemoved", listener);
          };
        }).api(),

        onTitleChanged: new SingletonEventManager(context, "history.onTitleChanged", fire => {
          let listener = (event, data) => {
            fire.sync(data);
          };

          getObserver().on("titleChanged", listener);
          return () => {
            getObserver().off("titleChanged", listener);
          };
        }).api(),
      },
    };
  }
};
