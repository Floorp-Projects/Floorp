/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { flatten, uniqBy } from "lodash";

import {
  stringToSourceActorId,
  type SourceActor,
} from "../../reducers/source-actors";
import { insertSourceActors } from "../../actions/source-actors";
import { makeSourceId } from "../../client/firefox/create";
import { toggleBlackBox } from "./blackbox";
import { syncBreakpoint } from "../breakpoints";
import { loadSourceText } from "./loadSourceText";
import { togglePrettyPrint } from "./prettyPrint";
import { selectLocation, setBreakableLines } from "../sources";
import {
  getRawSourceURL,
  isPrettyURL,
  isUrlExtension,
  isInlineScript,
} from "../../utils/source";
import {
  getBlackBoxList,
  getSource,
  getSourceFromId,
  hasSourceActor,
  getSourceByActorId,
  getPendingSelectedLocation,
  getPendingBreakpointsForSource,
  getContext,
  isSourceLoadingOrLoaded,
} from "../../selectors";

import { prefs } from "../../utils/prefs";
import sourceQueue from "../../utils/source-queue";
import { validateNavigateContext, ContextError } from "../../utils/context";

import type {
  Source,
  Context,
  OriginalSourceData,
  GeneratedSourceData,
  QueuedSourceData,
} from "../../types";
import type { Action, ThunkArgs } from "../types";

function loadSourceMaps(cx: Context, sources: SourceActor[]) {
  return async function({
    dispatch,
    sourceMaps,
  }: ThunkArgs): Promise<?(Promise<Source>[])> {
    try {
      const sourceList = await Promise.all(
        sources.map(async sourceActor => {
          const originalSources = await dispatch(
            loadSourceMap(cx, sourceActor)
          );
          sourceQueue.queueSources(
            originalSources.map(data => ({
              type: "original",
              data,
            }))
          );
          return originalSources;
        })
      );

      await sourceQueue.flush();

      return flatten(sourceList);
    } catch (error) {
      if (!(error instanceof ContextError)) {
        throw error;
      }
    }
  };
}

/**
 * @memberof actions/sources
 * @static
 */
function loadSourceMap(cx: Context, sourceActor: SourceActor) {
  return async function({
    dispatch,
    getState,
    sourceMaps,
  }: ThunkArgs): Promise<OriginalSourceData[]> {
    if (!prefs.clientSourceMapsEnabled || !sourceActor.sourceMapURL) {
      return [];
    }

    let data = null;
    try {
      // Unable to correctly type the result of a spread on a union type.
      // See https://github.com/facebook/flow/pull/7298
      let url = sourceActor.url || "";
      if (!sourceActor.url && typeof sourceActor.introductionUrl === "string") {
        // If the source was dynamically generated (via eval, dynamically
        // created script elements, and so forth), it won't have a URL, so that
        // it is not collapsed into other sources from the same place. The
        // introduction URL will include the point it was constructed at,
        // however, so use that for resolving any source maps in the source.
        url = sourceActor.introductionUrl;
      }

      // Ignore sourceMapURL on scripts that are part of HTML files, since
      // we currently treat sourcemaps as Source-wide, not SourceActor-specific.
      const source = getSourceByActorId(getState(), sourceActor.id);
      if (source) {
        data = await sourceMaps.getOriginalURLs({
          // Using source ID here is historical and eventually we'll want to
          // switch to all of this being per-source-actor.
          id: source.id,
          url,
          sourceMapURL: sourceActor.sourceMapURL || "",
          isWasm: sourceActor.introductionType === "wasm",
        });
      }
    } catch (e) {
      console.error(e);
    }

    if (!data) {
      // If this source doesn't have a sourcemap, enable it for pretty printing
      dispatch(
        ({
          type: "CLEAR_SOURCE_ACTOR_MAP_URL",
          cx,
          id: sourceActor.id,
        }: Action)
      );
      return [];
    }

    validateNavigateContext(getState(), cx);
    return data;
  };
}

// If a request has been made to show this source, go ahead and
// select it.
function checkSelectedSource(cx: Context, sourceId: string) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const source = getSource(getState(), sourceId);
    const pendingLocation = getPendingSelectedLocation(getState());

    if (!pendingLocation || !pendingLocation.url || !source || !source.url) {
      return;
    }

    const pendingUrl = pendingLocation.url;
    const rawPendingUrl = getRawSourceURL(pendingUrl);

    if (rawPendingUrl === source.url) {
      if (isPrettyURL(pendingUrl)) {
        const prettySource = await dispatch(togglePrettyPrint(cx, source.id));
        return dispatch(checkPendingBreakpoints(cx, prettySource.id));
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

function checkPendingBreakpoints(cx: Context, sourceId: string) {
  return async ({ dispatch, getState }: ThunkArgs) => {
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

    await Promise.all(
      pendingBreakpoints.map(bp => {
        return dispatch(syncBreakpoint(cx, sourceId, bp));
      })
    );
  };
}

function restoreBlackBoxedSources(cx: Context, sources: Source[]) {
  return async ({ dispatch }: ThunkArgs) => {
    const tabs = getBlackBoxList();
    if (tabs.length == 0) {
      return;
    }
    for (const source of sources) {
      if (tabs.includes(source.url) && !source.isBlackBoxed) {
        dispatch(toggleBlackBox(cx, source));
      }
    }
  };
}

export function newQueuedSources(sourceInfo: Array<QueuedSourceData>) {
  return async ({ dispatch }: ThunkArgs) => {
    const generated = [];
    const original = [];
    for (const source of sourceInfo) {
      if (source.type === "generated") {
        generated.push(source.data);
      } else {
        original.push(source.data);
      }
    }

    if (generated.length > 0) {
      await dispatch(newGeneratedSources(generated));
    }
    if (original.length > 0) {
      await dispatch(newOriginalSources(original));
    }
  };
}

export function newOriginalSource(sourceInfo: OriginalSourceData) {
  return async ({ dispatch }: ThunkArgs) => {
    const sources = await dispatch(newOriginalSources([sourceInfo]));
    return sources[0];
  };
}
export function newOriginalSources(sourceInfo: Array<OriginalSourceData>) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    sourceInfo = sourceInfo.filter(({ id }) => !getSource(getState(), id));
    sourceInfo = uniqBy(sourceInfo, ({ id }) => id);

    const sources: Array<Source> = sourceInfo.map(({ id, url }) => ({
      id,
      url,
      relativeUrl: url,
      isPrettyPrinted: false,
      isWasm: false,
      isBlackBoxed: false,
      introductionUrl: null,
      introductionType: undefined,
      isExtension: false,
      extensionName: null,
    }));

    const cx = getContext(getState());
    dispatch(addSources(cx, sources));

    await dispatch(checkNewSources(cx, sources));

    for (const source of sources) {
      dispatch(checkPendingBreakpoints(cx, source.id));
    }

    return sources;
  };
}

export function newGeneratedSource(sourceInfo: GeneratedSourceData) {
  return async ({ dispatch }: ThunkArgs) => {
    const sources = await dispatch(newGeneratedSources([sourceInfo]));
    return sources[0];
  };
}
export function newGeneratedSources(sourceInfo: Array<GeneratedSourceData>) {
  return async ({
    dispatch,
    getState,
    client,
  }: ThunkArgs): Promise<Array<Source>> => {
    const supportsWasm = client.hasWasmSupport();

    const resultIds = [];
    const newSourcesObj = {};
    const newSourceActors: Array<SourceActor> = [];

    for (const { thread, source, id } of sourceInfo) {
      const newId = id || makeSourceId(source);

      if (!getSource(getState(), newId) && !newSourcesObj[newId]) {
        newSourcesObj[newId] = {
          id: newId,
          url: source.url,
          relativeUrl: source.url,
          isPrettyPrinted: false,
          extensionName: source.extensionName,
          introductionUrl: source.introductionUrl,
          introductionType: source.introductionType,
          isBlackBoxed: false,
          isWasm: !!supportsWasm && source.introductionType === "wasm",
          isExtension: (source.url && isUrlExtension(source.url)) || false,
        };
      }

      const actorId = stringToSourceActorId(source.actor);

      // We are sometimes notified about a new source multiple times if we
      // request a new source list and also get a source event from the server.
      if (!hasSourceActor(getState(), actorId)) {
        newSourceActors.push({
          id: actorId,
          actor: source.actor,
          thread,
          source: newId,
          isBlackBoxed: source.isBlackBoxed,
          sourceMapURL: source.sourceMapURL,
          url: source.url,
          introductionUrl: source.introductionUrl,
          introductionType: source.introductionType,
        });
      }

      resultIds.push(newId);
    }

    const newSources: Array<Source> = (Object.values(newSourcesObj): any[]);

    const cx = getContext(getState());
    dispatch(addSources(cx, newSources));
    dispatch(insertSourceActors(newSourceActors));

    for (const newSourceActor of newSourceActors) {
      // Fetch breakable lines for new HTML scripts
      // when the HTML file has started loading
      if (
        isInlineScript(newSourceActor) &&
        isSourceLoadingOrLoaded(getState(), newSourceActor.source)
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
      for (const source of newSources) {
        dispatch(checkPendingBreakpoints(cx, source.id));
      }
    })();

    return resultIds.map(id => getSourceFromId(getState(), id));
  };
}

function addSources(cx, sources: Array<Source>) {
  return ({ dispatch, getState }: ThunkArgs) => {
    dispatch({ type: "ADD_SOURCES", cx, sources });
  };
}

function checkNewSources(cx, sources: Source[]) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    for (const source of sources) {
      dispatch(checkSelectedSource(cx, source.id));
    }

    dispatch(restoreBlackBoxedSources(cx, sources));

    return sources;
  };
}
