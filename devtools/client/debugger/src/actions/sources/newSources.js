/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { insertSourceActors } from "../../actions/source-actors";
import {
  makeSourceId,
  createGeneratedSource,
  createSourceMapOriginalSource,
  createSourceActor,
} from "../../client/firefox/create";
import { toggleBlackBox } from "./blackbox";
import { syncBreakpoint } from "../breakpoints";
import { loadSourceText } from "./loadSourceText";
import { togglePrettyPrint } from "./prettyPrint";
import { selectLocation, setBreakableLines } from "../sources";

import {
  getRawSourceURL,
  isPrettyURL,
  isInlineScript,
} from "../../utils/source";
import {
  getBlackBoxRanges,
  getSource,
  getSourceFromId,
  hasSourceActor,
  getSourceByActorId,
  getPendingSelectedLocation,
  getPendingBreakpointsForSource,
  getContext,
  getSourceTextContent,
} from "../../selectors";

import { prefs } from "../../utils/prefs";
import sourceQueue from "../../utils/source-queue";
import { validateNavigateContext, ContextError } from "../../utils/context";

function loadSourceMaps(cx, sources) {
  return async function({ dispatch, sourceMaps }) {
    try {
      const sourceList = await Promise.all(
        sources.map(async sourceActor => {
          const originalSourcesInfo = await dispatch(
            loadSourceMap(cx, sourceActor)
          );
          originalSourcesInfo.forEach(
            sourceInfo => (sourceInfo.thread = sourceActor.thread)
          );
          sourceQueue.queueOriginalSources(originalSourcesInfo);
          return originalSourcesInfo;
        })
      );

      await sourceQueue.flush();

      return sourceList.flat();
    } catch (error) {
      if (!(error instanceof ContextError)) {
        throw error;
      }
    }

    return [];
  };
}

/**
 * @memberof actions/sources
 * @static
 */
function loadSourceMap(cx, sourceActor) {
  return async function({ dispatch, getState, sourceMaps }) {
    if (!prefs.clientSourceMapsEnabled || !sourceActor.sourceMapURL) {
      return [];
    }

    let data = null;
    try {
      // Ignore sourceMapURL on scripts that are part of HTML files, since
      // we currently treat sourcemaps as Source-wide, not SourceActor-specific.
      const source = getSourceByActorId(getState(), sourceActor.id);
      if (source) {
        data = await sourceMaps.getOriginalURLs({
          // Using source ID here is historical and eventually we'll want to
          // switch to all of this being per-source-actor.
          id: source.id,
          url: sourceActor.url || "",
          sourceMapBaseURL: sourceActor.sourceMapBaseURL || "",
          sourceMapURL: sourceActor.sourceMapURL || "",
          isWasm: sourceActor.introductionType === "wasm",
        });
      }
    } catch (e) {
      console.error(e);
    }

    if (!data) {
      // If this source doesn't have a sourcemap, enable it for pretty printing
      dispatch({
        type: "CLEAR_SOURCE_ACTOR_MAP_URL",
        cx,
        id: sourceActor.id,
      });
      return [];
    }

    validateNavigateContext(getState(), cx);
    return data;
  };
}

// If a request has been made to show this source, go ahead and
// select it.
function checkSelectedSource(cx, sourceId) {
  return async ({ dispatch, getState }) => {
    const state = getState();
    const pendingLocation = getPendingSelectedLocation(state);

    if (!pendingLocation || !pendingLocation.url) {
      return;
    }

    const source = getSource(state, sourceId);

    if (!source || !source.url) {
      return;
    }

    const pendingUrl = pendingLocation.url;
    const rawPendingUrl = getRawSourceURL(pendingUrl);

    if (rawPendingUrl === source.url) {
      if (isPrettyURL(pendingUrl)) {
        const prettySource = await dispatch(togglePrettyPrint(cx, source.id));
        dispatch(checkPendingBreakpoints(cx, prettySource.id));
        return;
      }

      await dispatch(
        selectLocation(cx, {
          sourceId: source.id,
          line:
            typeof pendingLocation.line === "number" ? pendingLocation.line : 0,
          column: pendingLocation.column,
        })
      );
    }
  };
}

function checkPendingBreakpoints(cx, sourceId) {
  return async ({ dispatch, getState }) => {
    // source may have been modified by selectLocation
    const source = getSource(getState(), sourceId);
    if (!source) {
      return;
    }

    const pendingBreakpoints = getPendingBreakpointsForSource(
      getState(),
      source
    );

    if (pendingBreakpoints.length === 0) {
      return;
    }

    // load the source text if there is a pending breakpoint for it
    await dispatch(loadSourceText({ cx, source }));

    await dispatch(setBreakableLines(cx, source.id));

    await Promise.all(
      pendingBreakpoints.map(bp => {
        return dispatch(syncBreakpoint(cx, sourceId, bp));
      })
    );
  };
}

function restoreBlackBoxedSources(cx, sources) {
  return async ({ dispatch, getState }) => {
    const currentRanges = getBlackBoxRanges(getState());

    if (!Object.keys(currentRanges).length) {
      return;
    }

    for (const source of sources) {
      const ranges = currentRanges[source.url];
      if (ranges) {
        // If the ranges is an empty then the whole source was blackboxed.
        await dispatch(toggleBlackBox(cx, source, true, ranges));
      }
    }
  };
}

// Wrapper around newOriginalSources, only used by tests
export function newOriginalSource(sourceInfo) {
  return async ({ dispatch }) => {
    const sources = await dispatch(newOriginalSources([sourceInfo]));
    return sources[0];
  };
}

export function newOriginalSources(originalSourcesInfo) {
  return async ({ dispatch, getState }) => {
    const state = getState();
    const seen = new Set();
    const sources = [];

    for (const { id, url, thread } of originalSourcesInfo) {
      if (seen.has(id) || getSource(state, id)) {
        continue;
      }

      seen.add(id);

      sources.push(createSourceMapOriginalSource(id, url, thread));
    }

    const cx = getContext(state);
    dispatch(addSources(cx, sources));

    await dispatch(checkNewSources(cx, sources));

    for (const source of sources) {
      dispatch(checkPendingBreakpoints(cx, source.id));
    }

    return sources;
  };
}

// Wrapper around newGeneratedSources, only used by tests
export function newGeneratedSource(sourceInfo) {
  return async ({ dispatch }) => {
    const sources = await dispatch(newGeneratedSources([sourceInfo]));
    return sources[0];
  };
}

export function newGeneratedSources(sourceResources) {
  return async ({ dispatch, getState, client }) => {
    if (!sourceResources.length) {
      return [];
    }

    const resultIds = [];
    const newSourcesObj = {};
    const newSourceActors = [];

    for (const sourceResource of sourceResources) {
      // By the time we process the sources, the related target
      // might already have been destroyed. It means that the sources
      // are also about to be destroyed, so ignore them.
      // (This is covered by browser_toolbox_backward_forward_navigation.js)
      if (sourceResource.targetFront.isDestroyed()) {
        continue;
      }
      const id = makeSourceId(sourceResource);

      if (!getSource(getState(), id) && !newSourcesObj[id]) {
        newSourcesObj[id] = createGeneratedSource(sourceResource);
      }

      const actorId = sourceResource.actor;

      // We are sometimes notified about a new source multiple times if we
      // request a new source list and also get a source event from the server.
      if (!hasSourceActor(getState(), actorId)) {
        newSourceActors.push(
          createSourceActor(
            sourceResource,
            getSource(getState(), id) || newSourcesObj[id]
          )
        );
      }

      resultIds.push(id);
    }

    const newSources = Object.values(newSourcesObj);

    const cx = getContext(getState());
    dispatch(addSources(cx, newSources));
    dispatch(insertSourceActors(newSourceActors));

    for (const newSourceActor of newSourceActors) {
      // Fetch breakable lines for new HTML scripts
      // when the HTML file has started loading
      if (
        isInlineScript(newSourceActor) &&
        getSourceTextContent(getState(), newSourceActor.source) != null
      ) {
        dispatch(setBreakableLines(cx, newSourceActor.source)).catch(error => {
          if (!(error instanceof ContextError)) {
            throw error;
          }
        });
      }
    }
    await dispatch(checkNewSources(cx, newSources));

    (async () => {
      await dispatch(loadSourceMaps(cx, newSourceActors));

      // We would like to sync breakpoints after we are done
      // loading source maps as sometimes generated and original
      // files share the same paths.
      for (const { source } of newSourceActors) {
        dispatch(checkPendingBreakpoints(cx, source));
      }
    })();

    return resultIds.map(id => getSourceFromId(getState(), id));
  };
}

function addSources(cx, sources) {
  return ({ dispatch, getState }) => {
    dispatch({ type: "ADD_SOURCES", cx, sources });
  };
}

function checkNewSources(cx, sources) {
  return async ({ dispatch, getState }) => {
    for (const source of sources) {
      dispatch(checkSelectedSource(cx, source.id));
    }

    await dispatch(restoreBlackBoxedSources(cx, sources));

    return sources;
  };
}
