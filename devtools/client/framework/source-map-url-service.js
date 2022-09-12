/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const SOURCE_MAP_PREF = "devtools.source-map.client-service.enabled";

/**
 * A simple service to track source actors and keep a mapping between
 * original URLs and objects holding the source or style actor's ID
 * (which is used as a cookie by the devtools-source-map service) and
 * the source map URL.
 *
 * @param {object} commands
 *        The commands object with all interfaces defined from devtools/shared/commands/
 * @param {SourceMapService} sourceMapService
 *        The devtools-source-map functions
 */
class SourceMapURLService {
  constructor(commands, sourceMapService) {
    this._commands = commands;
    this._sourceMapService = sourceMapService;

    this._prefValue = Services.prefs.getBoolPref(SOURCE_MAP_PREF);
    this._pendingIDSubscriptions = new Map();
    this._pendingURLSubscriptions = new Map();
    this._urlToIDMap = new Map();
    this._mapsById = new Map();
    this._sourcesLoading = null;
    this._onResourceAvailable = this._onResourceAvailable.bind(this);
    this._runningCallback = false;

    this._syncPrevValue = this._syncPrevValue.bind(this);
    this._clearAllState = this._clearAllState.bind(this);

    Services.prefs.addObserver(SOURCE_MAP_PREF, this._syncPrevValue);
  }

  destroy() {
    Services.prefs.removeObserver(SOURCE_MAP_PREF, this._syncPrevValue);

    this._clearAllState();

    const { resourceCommand } = this._commands;
    try {
      resourceCommand.unwatchResources(
        [
          resourceCommand.TYPES.STYLESHEET,
          resourceCommand.TYPES.SOURCE,
          resourceCommand.TYPES.DOCUMENT_EVENT,
        ],
        { onAvailable: this._onResourceAvailable }
      );
    } catch (e) {
      // If unwatchResources is called before finishing process of watchResources,
      // it throws an error during stopping listener.
    }

    this._sourcesLoading = null;
  }

  /**
   * Subscribe to notifications about the original location of a given
   * generated location, as it may not be known at this time, may become
   * available at some unknown time in the future, or may change from one
   * location to another.
   *
   * @param {string} id The actor ID of the source.
   * @param {number} line The line number in the source.
   * @param {number} column The column number in the source.
   * @param {Function} callback A callback that may eventually be passed an
   *      an object with url/line/column properties specifying a location in
   *      the original file, or null if no particular original location could
   *      be found. The callback will run synchronously if the location is
   *      already know to the URL service.
   *
   * @return {Function} A function to call to remove this subscription. The
   *      "callback" argument is guaranteed to never run once unsubscribed.
   */
  subscribeByID(id, line, column, callback) {
    this._ensureAllSourcesPopulated();

    let pending = this._pendingIDSubscriptions.get(id);
    if (!pending) {
      pending = new Set();
      this._pendingIDSubscriptions.set(id, pending);
    }
    const entry = {
      line,
      column,
      callback,
      unsubscribed: false,
      owner: pending,
    };
    pending.add(entry);

    const map = this._mapsById.get(id);
    if (map) {
      this._flushPendingIDSubscriptionsToMapQueries(map);
    }

    return () => {
      entry.unsubscribed = true;
      entry.owner.delete(entry);
    };
  }

  /**
   * Subscribe to notifications about the original location of a given
   * generated location, as it may not be known at this time, may become
   * available at some unknown time in the future, or may change from one
   * location to another.
   *
   * @param {string} id The actor ID of the source.
   * @param {number} line The line number in the source.
   * @param {number} column The column number in the source.
   * @param {Function} callback A callback that may eventually be passed an
   *      an object with url/line/column properties specifying a location in
   *      the original file, or null if no particular original location could
   *      be found. The callback will run synchronously if the location is
   *      already know to the URL service.
   *
   * @return {Function} A function to call to remove this subscription. The
   *      "callback" argument is guaranteed to never run once unsubscribed.
   */
  subscribeByURL(url, line, column, callback) {
    this._ensureAllSourcesPopulated();

    let pending = this._pendingURLSubscriptions.get(url);
    if (!pending) {
      pending = new Set();
      this._pendingURLSubscriptions.set(url, pending);
    }
    const entry = {
      line,
      column,
      callback,
      unsubscribed: false,
      owner: pending,
    };
    pending.add(entry);

    const id = this._urlToIDMap.get(url);
    if (id) {
      this._convertPendingURLSubscriptionsToID(url, id);
      const map = this._mapsById.get(id);
      if (map) {
        this._flushPendingIDSubscriptionsToMapQueries(map);
      }
    }

    return () => {
      entry.unsubscribed = true;
      entry.owner.delete(entry);
    };
  }

  /**
   * Subscribe generically based on either an ID or a URL.
   *
   * In an ideal world we'd always know which of these to use, but there are
   * still cases where end up with a mixture of both, so this is provided as
   * a helper. If you can specifically use one of these, please do that
   * instead however.
   */
  subscribeByLocation({ id, url, line, column }, callback) {
    if (id) {
      return this.subscribeByID(id, line, column, callback);
    }

    return this.subscribeByURL(url, line, column, callback);
  }

  /**
   * Tell the URL service than some external entity has registered a sourcemap
   * in the worker for one of the source files.
   *
   * @param {string} id The actor ID of the source that had the map registered.
   */
  async newSourceMapCreated(id) {
    await this._ensureAllSourcesPopulated();

    const map = this._mapsById.get(id);
    if (!map) {
      // State could have been cleared.
      return;
    }

    map.loaded = Promise.resolve();
    for (const query of map.queries.values()) {
      query.action = null;
      query.result = null;
      if (this._prefValue) {
        this._dispatchQuery(query);
      }
    }
  }

  _syncPrevValue() {
    this._prefValue = Services.prefs.getBoolPref(SOURCE_MAP_PREF);

    for (const map of this._mapsById.values()) {
      for (const query of map.queries.values()) {
        this._ensureSubscribersSynchronized(query);
      }
    }
  }

  _clearAllState() {
    this._sourceMapService.clearSourceMaps();
    this._pendingIDSubscriptions.clear();
    this._pendingURLSubscriptions.clear();
    this._urlToIDMap.clear();
  }

  _onNewJavascript(source) {
    const { url, actor: id, sourceMapBaseURL, sourceMapURL } = source;

    this._onNewSource(id, url, sourceMapURL, sourceMapBaseURL);
  }

  _onNewStyleSheet(sheet) {
    const {
      href,
      nodeHref,
      sourceMapBaseURL,
      sourceMapURL,
      resourceId: id,
    } = sheet;
    const url = href || nodeHref;

    this._onNewSource(id, url, sourceMapURL, sourceMapBaseURL);
  }

  _onNewSource(id, url, sourceMapURL, sourceMapBaseURL) {
    this._urlToIDMap.set(url, id);
    this._convertPendingURLSubscriptionsToID(url, id);

    let map = this._mapsById.get(id);
    if (!map) {
      map = {
        id,
        url,
        sourceMapURL,
        sourceMapBaseURL,
        loaded: null,
        queries: new Map(),
      };
      this._mapsById.set(id, map);
    } else if (
      map.id !== id &&
      map.url !== url &&
      map.sourceMapURL !== sourceMapURL &&
      map.sourceMapBaseURL !== sourceMapBaseURL
    ) {
      console.warn(
        `Attempted to load populate sourcemap for source ${id} multiple times`
      );
    }

    this._flushPendingIDSubscriptionsToMapQueries(map);
  }

  _buildQuery(map, line, column) {
    const key = `${line}:${column}`;
    let query = map.queries.get(key);
    if (!query) {
      query = {
        map,
        line,
        column,
        subscribers: new Set(),
        action: null,
        result: null,
        mostRecentEmitted: null,
      };
      map.queries.set(key, query);
    }
    return query;
  }

  _dispatchQuery(query, newSubscribers = null) {
    if (!this._prefValue) {
      throw new Error("This function should only be called if the pref is on.");
    }

    if (!query.action) {
      const { map } = query;

      // Call getOriginalURLs to make sure the source map has been
      // fetched.  We don't actually need the result of this though.
      if (!map.loaded) {
        map.loaded = this._sourceMapService.getOriginalURLs({
          id: map.id,
          url: map.url,
          sourceMapBaseURL: map.sourceMapBaseURL,
          sourceMapURL: map.sourceMapURL,
        });
      }

      const action = (async () => {
        let result = null;
        try {
          await map.loaded;

          const position = await this._sourceMapService.getOriginalLocation({
            sourceId: map.id,
            line: query.line,
            column: query.column,
          });
          if (position && position.sourceId !== map.id) {
            result = {
              url: position.sourceUrl,
              line: position.line,
              column: position.column,
            };
          }
        } finally {
          // If this action was dispatched and then the file was pretty-printed
          // we want to ignore the result since the query has restarted.
          if (action === query.action) {
            // It is important that we consistently set the query result and
            // trigger the subscribers here in order to maintain the invariant
            // that if 'result' is truthy, then the subscribers will have run.
            const position = result;
            query.result = { position };
            this._ensureSubscribersSynchronized(query);
          }
        }
      })();
      query.action = action;
    }

    this._ensureSubscribersSynchronized(query);
  }

  _ensureSubscribersSynchronized(query) {
    // Synchronize the subscribers with the pref-disabled state if they need it.
    if (!this._prefValue) {
      if (query.mostRecentEmitted) {
        query.mostRecentEmitted = null;
        this._dispatchSubscribers(null, query.subscribers);
      }
      return;
    }

    // Synchronize the subscribers with the newest computed result if they
    // need it.
    const { result } = query;
    if (result && query.mostRecentEmitted !== result.position) {
      query.mostRecentEmitted = result.position;
      this._dispatchSubscribers(result.position, query.subscribers);
    }
  }

  _dispatchSubscribers(position, subscribers) {
    // We copy the subscribers before iterating because something could be
    // removed while we're calling the callbacks, which is also why we check
    // the 'unsubscribed' flag.
    for (const subscriber of Array.from(subscribers)) {
      if (subscriber.unsubscribed) {
        continue;
      }

      if (this._runningCallback) {
        console.error(
          "The source map url service does not support reentrant subscribers."
        );
        continue;
      }

      try {
        this._runningCallback = true;

        const { callback } = subscriber;
        callback(position ? { ...position } : null);
      } catch (err) {
        console.error("Error in source map url service subscriber", err);
      } finally {
        this._runningCallback = false;
      }
    }
  }

  _flushPendingIDSubscriptionsToMapQueries(map) {
    const subscriptions = this._pendingIDSubscriptions.get(map.id);
    if (!subscriptions || subscriptions.size === 0) {
      return;
    }
    this._pendingIDSubscriptions.delete(map.id);

    for (const entry of subscriptions) {
      const query = this._buildQuery(map, entry.line, entry.column);

      const { subscribers } = query;

      entry.owner = subscribers;
      subscribers.add(entry);

      if (query.mostRecentEmitted) {
        // Maintain the invariant that if a query has emitted a value, then
        // _all_ subscribers will have received that value.
        this._dispatchSubscribers(query.mostRecentEmitted, [entry]);
      }

      if (this._prefValue) {
        this._dispatchQuery(query);
      }
    }
  }

  _ensureAllSourcesPopulated() {
    if (!this._prefValue || this._commands.descriptorFront.isWorkerDescriptor) {
      return null;
    }

    if (!this._sourcesLoading) {
      const { resourceCommand } = this._commands;
      const { STYLESHEET, SOURCE, DOCUMENT_EVENT } = resourceCommand.TYPES;

      const onResources = resourceCommand.watchResources(
        [STYLESHEET, SOURCE, DOCUMENT_EVENT],
        {
          onAvailable: this._onResourceAvailable,
        }
      );
      this._sourcesLoading = onResources;
    }

    return this._sourcesLoading;
  }

  waitForSourcesLoading() {
    if (this._sourcesLoading) {
      return this._sourcesLoading;
    }
    return Promise.resolve();
  }

  _onResourceAvailable(resources) {
    const { resourceCommand } = this._commands;
    const { STYLESHEET, SOURCE, DOCUMENT_EVENT } = resourceCommand.TYPES;
    for (const resource of resources) {
      // Only consider top level document, and ignore remote iframes top document
      if (
        resource.resourceType == DOCUMENT_EVENT &&
        resource.name == "will-navigate" &&
        resource.isTopLevel
      ) {
        this._clearAllState();
      } else if (resource.resourceType == STYLESHEET) {
        this._onNewStyleSheet(resource);
      } else if (resource.resourceType == SOURCE) {
        this._onNewJavascript(resource);
      }
    }
  }

  _convertPendingURLSubscriptionsToID(url, id) {
    const urlSubscriptions = this._pendingURLSubscriptions.get(url);
    if (!urlSubscriptions) {
      return;
    }
    this._pendingURLSubscriptions.delete(url);

    let pending = this._pendingIDSubscriptions.get(id);
    if (!pending) {
      pending = new Set();
      this._pendingIDSubscriptions.set(id, pending);
    }
    for (const entry of urlSubscriptions) {
      entry.owner = pending;
      pending.add(entry);
    }
  }
}

exports.SourceMapURLService = SourceMapURLService;
