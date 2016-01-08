/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require('../constants');
const promise = require('promise');
const { dumpn } = require("devtools/shared/DevToolsUtils");
const { PROMISE, HISTOGRAM_ID } = require('devtools/client/shared/redux/middleware/promise');
const { getSource, getSourceText } = require('../queries');

const NEW_SOURCE_IGNORED_URLS = ["debugger eval code", "XStringBundle"];
const FETCH_SOURCE_RESPONSE_DELAY = 200; // ms

function getSourceClient(source) {
  return gThreadClient.source(source);
}

/**
 * Handler for the debugger client's unsolicited newSource notification.
 */
function newSource(source) {
  return dispatch => {
    // Ignore bogus scripts, e.g. generated from 'clientEvaluate' packets.
    if (NEW_SOURCE_IGNORED_URLS.indexOf(source.url) != -1) {
      return;
    }

    // Signal that a new source has been added.
    window.emit(EVENTS.NEW_SOURCE);

    return dispatch({
      type: constants.ADD_SOURCE,
      source: source
    });
  };
}

function selectSource(source, opts) {
  return (dispatch, getState) => {
    if (!gThreadClient) {
      // No connection, do nothing. This happens when the debugger is
      // shut down too fast and it tries to display a default source.
      return;
    }

    source = getSource(getState(), source.actor);

    // Make sure to start a request to load the source text.
    dispatch(loadSourceText(source));

    dispatch({
      type: constants.SELECT_SOURCE,
      source: source,
      opts: opts
    });
  };
}

function loadSources() {
  return {
    type: constants.LOAD_SOURCES,
    [PROMISE]: Task.spawn(function*() {
      const response = yield gThreadClient.getSources();

      // Top-level breakpoints may pause the entire loading process
      // because scripts are executed as they are loaded, so the
      // engine may pause in the middle of loading all the sources.
      // This is relatively harmless, as individual `newSource`
      // notifications are fired for each script and they will be
      // added to the UI through that.
      if (!response.sources) {
        dumpn(
          "Error getting sources, probably because a top-level " +
          "breakpoint was hit while executing them"
        );
        return;
      }

      // Ignore bogus scripts, e.g. generated from 'clientEvaluate' packets.
      return response.sources.filter(source => {
        return NEW_SOURCE_IGNORED_URLS.indexOf(source.url) === -1;
      });
    })
  }
}

/**
 * Set the black boxed status of the given source.
 *
 * @param Object aSource
 *        The source form.
 * @param bool aBlackBoxFlag
 *        True to black box the source, false to un-black box it.
 * @returns Promise
 *          A promize that resolves to [aSource, isBlackBoxed] or rejects to
 *          [aSource, error].
 */
function blackbox(source, shouldBlackBox) {
  const client = getSourceClient(source);

  return {
    type: constants.BLACKBOX,
    source: source,
    [PROMISE]: Task.spawn(function*() {
      yield shouldBlackBox ? client.blackBox() : client.unblackBox();
      return {
        isBlackBoxed: shouldBlackBox
      }
    })
  };
}

/**
 * Toggle the pretty printing of a source's text. All subsequent calls to
 * |getText| will return the pretty-toggled text. Nothing will happen for
 * non-javascript files.
 *
 * @param Object aSource
 *        The source form from the RDP.
 * @returns Promise
 *          A promise that resolves to [aSource, prettyText] or rejects to
 *          [aSource, error].
 */
function togglePrettyPrint(source) {
  return (dispatch, getState) => {
    const sourceClient = getSourceClient(source);
    const wantPretty = !source.isPrettyPrinted;

    return dispatch({
      type: constants.TOGGLE_PRETTY_PRINT,
      source: source,
      [PROMISE]: Task.spawn(function*() {
        let response;

        // Only attempt to pretty print JavaScript sources.
        const sourceText = getSourceText(getState(), source.actor);
        const contentType = sourceText ? sourceText.contentType : null;
        if (!SourceUtils.isJavaScript(source.url, contentType)) {
          throw new Error("Can't prettify non-javascript files.");
        }

        if (wantPretty) {
          response = yield sourceClient.prettyPrint(Prefs.editorTabSize);
        }
        else {
          response = yield sourceClient.disablePrettyPrint();
        }

        // Remove the cached source AST from the Parser, to avoid getting
        // wrong locations when searching for functions.
        DebuggerController.Parser.clearSource(source.url);

        return {
          isPrettyPrinted: wantPretty,
          text: response.source,
          contentType: response.contentType
        };
      })
    });
  };
}

function loadSourceText(source) {
  return (dispatch, getState) => {
    // Fetch the source text only once.
    let textInfo = getSourceText(getState(), source.actor);
    if (textInfo) {
      // It's already loaded or is loading
      return promise.resolve(textInfo);
    }

    const sourceClient = getSourceClient(source);

    return dispatch({
      type: constants.LOAD_SOURCE_TEXT,
      source: source,
      [PROMISE]: Task.spawn(function*() {
        let transportType = gClient.localTransport ? "_LOCAL" : "_REMOTE";
        let histogramId = "DEVTOOLS_DEBUGGER_DISPLAY_SOURCE" + transportType + "_MS";
        let histogram = Services.telemetry.getHistogramById(histogramId);
        let startTime = Date.now();

        const response = yield sourceClient.source();

        histogram.add(Date.now() - startTime);

        // Automatically pretty print if enabled and the test is
        // detected to be "minified"
        if (Prefs.autoPrettyPrint &&
            !source.isPrettyPrinted &&
            SourceUtils.isMinified(source.actor, response.source)) {
          dispatch(togglePrettyPrint(source));
        }

        return { text: response.source,
                 contentType: response.contentType };
      })
    });
  }
}

/**
 * Starts fetching all the sources, silently.
 *
 * @param array aUrls
 *        The urls for the sources to fetch. If fetching a source's text
 *        takes too long, it will be discarded.
 * @return object
 *         A promise that is resolved after source texts have been fetched.
 */
function getTextForSources(actors) {
  return (dispatch, getState) => {
    let deferred = promise.defer();
    let pending = new Set(actors);
    let fetched = [];

    // Can't use promise.all, because if one fetch operation is rejected, then
    // everything is considered rejected, thus no other subsequent source will
    // be getting fetched. We don't want that. Something like Q's allSettled
    // would work like a charm here.

    // Try to fetch as many sources as possible.
    for (let actor of actors) {
      let source = getSource(getState(), actor);
      dispatch(loadSourceText(source)).then(({ text, contentType }) => {
        onFetch([source, text, contentType]);
      }, err => {
        onError(source, err);
      });
    }

    setTimeout(onTimeout, FETCH_SOURCE_RESPONSE_DELAY);

    /* Called if fetching a source takes too long. */
    function onTimeout() {
      pending = new Set();
      maybeFinish();
    }

    /* Called if fetching a source finishes successfully. */
    function onFetch([aSource, aText, aContentType]) {
      // If fetching the source has previously timed out, discard it this time.
      if (!pending.has(aSource.actor)) {
        return;
      }
      pending.delete(aSource.actor);
      fetched.push([aSource.actor, aText, aContentType]);
      maybeFinish();
    }

    /* Called if fetching a source failed because of an error. */
    function onError([aSource, aError]) {
      pending.delete(aSource.actor);
      maybeFinish();
    }

    /* Called every time something interesting happens while fetching sources. */
    function maybeFinish() {
      if (pending.size == 0) {
        // Sort the fetched sources alphabetically by their url.
        deferred.resolve(fetched.sort(([aFirst], [aSecond]) => aFirst > aSecond));
      }
    }

    return deferred.promise;
  };
}

module.exports = {
  newSource,
  selectSource,
  loadSources,
  blackbox,
  togglePrettyPrint,
  loadSourceText,
  getTextForSources
};
