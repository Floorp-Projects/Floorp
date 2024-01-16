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
import { syncPendingBreakpoint } from "../breakpoints/index";
import { loadSourceText } from "./loadSourceText";
import { prettyPrintAndSelectSource } from "./prettyPrint";
import { toggleSourceMapIgnoreList } from "../ui";
import { selectLocation, setBreakableLines } from "../sources/index";

import { getRawSourceURL, isPrettyURL } from "../../utils/source";
import { createLocation } from "../../utils/location";
import {
  getBlackBoxRanges,
  getSource,
  getSourceFromId,
  hasSourceActor,
  getSourceByActorId,
  getPendingSelectedLocation,
  getPendingBreakpointsForSource,
} from "../../selectors/index";

import { prefs } from "../../utils/prefs";
import sourceQueue from "../../utils/source-queue";
import { validateSourceActor, ContextError } from "../../utils/context";

function loadSourceMaps(sources) {
  return async function ({ dispatch }) {
    try {
      const sourceList = await Promise.all(
        sources.map(async sourceActor => {
          const originalSourcesInfo = await dispatch(
            loadSourceMap(sourceActor)
          );
          originalSourcesInfo.forEach(
            sourcesInfo => (sourcesInfo.sourceActor = sourceActor)
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
function loadSourceMap(sourceActor) {
  return async function ({ dispatch, getState, sourceMapLoader, panel }) {
    if (!prefs.clientSourceMapsEnabled || !sourceActor.sourceMapURL) {
      return [];
    }

    let sources, ignoreListUrls, resolvedSourceMapURL, exception;
    try {
      // Ignore sourceMapURL on scripts that are part of HTML files, since
      // we currently treat sourcemaps as Source-wide, not SourceActor-specific.
      const source = getSourceByActorId(getState(), sourceActor.id);
      if (source) {
        ({ sources, ignoreListUrls, resolvedSourceMapURL, exception } =
          await sourceMapLoader.loadSourceMap({
            // Using source ID here is historical and eventually we'll want to
            // switch to all of this being per-source-actor.
            id: source.id,
            url: sourceActor.url || "",
            sourceMapBaseURL: sourceActor.sourceMapBaseURL || "",
            sourceMapURL: sourceActor.sourceMapURL || "",
            isWasm: sourceActor.introductionType === "wasm",
          }));
      }
    } catch (e) {
      exception = `Internal error: ${e.message}`;
    }

    if (resolvedSourceMapURL) {
      dispatch({
        type: "RESOLVED_SOURCEMAP_URL",
        sourceActor,
        resolvedSourceMapURL,
      });
    }

    if (ignoreListUrls?.length) {
      dispatch({
        type: "ADD_SOURCEMAP_IGNORE_LIST_SOURCES",
        ignoreListUrls,
      });
    }

    if (exception) {
      // Catch all errors and log them to the Web Console for users to see.
      const message = L10N.getFormatStr(
        "toolbox.sourceMapFailure",
        exception,
        sourceActor.url,
        sourceActor.sourceMapURL
      );
      panel.toolbox.commands.targetCommand.targetFront.logWarningInPage(
        message,
        "source map",
        resolvedSourceMapURL
      );

      dispatch({
        type: "SOURCE_MAP_ERROR",
        sourceActor,
        errorMessage: exception,
      });

      // If this source doesn't have a sourcemap or there are no original files
      // existing, enable it for pretty printing
      dispatch({
        type: "CLEAR_SOURCE_ACTOR_MAP_URL",
        sourceActor,
      });
      return [];
    }

    // Before dispatching this action, ensure that the related sourceActor is still registered
    validateSourceActor(getState(), sourceActor);
    return sources;
  };
}

// If a request has been made to show this source, go ahead and
// select it.
function checkSelectedSource(sourceId) {
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
        const prettySource = await dispatch(prettyPrintAndSelectSource(source));
        dispatch(checkPendingBreakpoints(prettySource, null));
        return;
      }

      await dispatch(
        selectLocation(
          createLocation({
            source,
            line:
              typeof pendingLocation.line === "number"
                ? pendingLocation.line
                : 0,
            column: pendingLocation.column,
          })
        )
      );
    }
  };
}

function checkPendingBreakpoints(source, sourceActor) {
  return async ({ dispatch, getState }) => {
    const pendingBreakpoints = getPendingBreakpointsForSource(
      getState(),
      source
    );

    if (pendingBreakpoints.length === 0) {
      return;
    }

    // load the source text if there is a pending breakpoint for it
    await dispatch(loadSourceText(source, sourceActor));
    await dispatch(setBreakableLines(createLocation({ source, sourceActor })));

    await Promise.all(
      pendingBreakpoints.map(pendingBp => {
        return dispatch(syncPendingBreakpoint(source, pendingBp));
      })
    );
  };
}

function restoreBlackBoxedSources(sources) {
  return async ({ dispatch, getState }) => {
    const currentRanges = getBlackBoxRanges(getState());

    if (!Object.keys(currentRanges).length) {
      return;
    }

    for (const source of sources) {
      const ranges = currentRanges[source.url];
      if (ranges) {
        // If the ranges is an empty then the whole source was blackboxed.
        await dispatch(toggleBlackBox(source, true, ranges));
      }
    }

    if (prefs.sourceMapIgnoreListEnabled) {
      await dispatch(toggleSourceMapIgnoreList(true));
    }
  };
}

export function newOriginalSources(originalSourcesInfo) {
  return async ({ dispatch, getState }) => {
    const state = getState();
    const seen = new Set();

    const actors = [];
    const actorsSources = {};

    for (const { id, url, sourceActor } of originalSourcesInfo) {
      if (seen.has(id) || getSource(state, id)) {
        continue;
      }
      seen.add(id);

      if (!actorsSources[sourceActor.actor]) {
        actors.push(sourceActor);
        actorsSources[sourceActor.actor] = [];
      }

      actorsSources[sourceActor.actor].push(
        createSourceMapOriginalSource(id, url)
      );
    }

    // Add the original sources per the generated source actors that
    // they are primarily from.
    actors.forEach(sourceActor => {
      dispatch({
        type: "ADD_ORIGINAL_SOURCES",
        originalSources: actorsSources[sourceActor.actor],
        generatedSourceActor: sourceActor,
      });
    });

    // Accumulate the sources back into one list
    const actorsSourcesValues = Object.values(actorsSources);
    let sources = [];
    if (actorsSourcesValues.length) {
      sources = actorsSourcesValues.reduce((acc, sourceList) =>
        acc.concat(sourceList)
      );
    }

    await dispatch(checkNewSources(sources));

    for (const source of sources) {
      dispatch(checkPendingBreakpoints(source, null));
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

    dispatch({ type: "ADD_SOURCES", sources: newSources });
    dispatch(insertSourceActors(newSourceActors));

    await dispatch(checkNewSources(newSources));

    (async () => {
      await dispatch(loadSourceMaps(newSourceActors));

      // We would like to sync breakpoints after we are done
      // loading source maps as sometimes generated and original
      // files share the same paths.
      for (const sourceActor of newSourceActors) {
        // For HTML pages, we fetch all new incoming inline script,
        // which will be related to one dedicated source actor.
        // Whereas, for regular sources, if we have many source actors,
        // this is for the same URL. And code expecting to have breakable lines
        // will request breakable lines for that particular source actor.
        if (sourceActor.sourceObject.isHTML) {
          await dispatch(
            setBreakableLines(
              createLocation({ source: sourceActor.sourceObject, sourceActor })
            )
          );
        }
        dispatch(
          checkPendingBreakpoints(sourceActor.sourceObject, sourceActor)
        );
      }
    })();

    return resultIds.map(id => getSourceFromId(getState(), id));
  };
}

function checkNewSources(sources) {
  return async ({ dispatch, getState }) => {
    for (const source of sources) {
      dispatch(checkSelectedSource(source.id));
    }

    await dispatch(restoreBlackBoxedSources(sources));

    return sources;
  };
}
