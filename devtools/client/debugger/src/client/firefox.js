/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { setupCommands, clientCommands } from "./firefox/commands";
import {
  setupCreate,
  createPause,
  prepareSourcePayload,
} from "./firefox/create";
import { features, prefs } from "../utils/prefs";

import { recordEvent } from "../utils/telemetry";
import sourceQueue from "../utils/source-queue";

let actions;
let targetCommand;
let resourceCommand;

export async function onConnect(commands, _resourceCommand, _actions, store) {
  actions = _actions;
  targetCommand = commands.targetCommand;
  resourceCommand = _resourceCommand;

  setupCommands(commands);
  setupCreate({ store });
  sourceQueue.initialize(actions);
  const { targetFront } = targetCommand;
  if (targetFront.isBrowsingContext || targetFront.isParentProcess) {
    targetCommand.listenForWorkers = true;
    if (targetFront.localTab && features.windowlessServiceWorkers) {
      targetCommand.listenForServiceWorkers = true;
      targetCommand.destroyServiceWorkersOnNavigation = true;
    }
    await targetCommand.startListening();
  }

  await targetCommand.watchTargets(
    targetCommand.ALL_TYPES,
    onTargetAvailable,
    onTargetDestroyed
  );

  // Use independant listeners for SOURCE and THREAD_STATE in order to ease
  // doing batching and notify about a set of SOURCE's in one redux action.
  await resourceCommand.watchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable: onSourceAvailable,
  });
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: onBreakpointAvailable,
  });
  await resourceCommand.watchResources([resourceCommand.TYPES.ERROR_MESSAGE], {
    onAvailable: actions.addExceptionFromResources,
  });
}

export function onDisconnect() {
  targetCommand.unwatchTargets(
    targetCommand.ALL_TYPES,
    onTargetAvailable,
    onTargetDestroyed
  );
  resourceCommand.unwatchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable: onSourceAvailable,
  });
  resourceCommand.unwatchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: onBreakpointAvailable,
  });
  resourceCommand.unwatchResources([resourceCommand.TYPES.ERROR_MESSAGE], {
    onAvailable: actions.addExceptionFromResources,
  });
  sourceQueue.clear();
}

async function onTargetAvailable({ targetFront, isTargetSwitching }) {
  const isBrowserToolbox = targetCommand.targetFront.isParentProcess;
  const isNonTopLevelFrameTarget =
    !targetFront.isTopLevel &&
    targetFront.targetType === targetCommand.TYPES.FRAME;

  if (isBrowserToolbox && isNonTopLevelFrameTarget) {
    // In the BrowserToolbox, non-top-level frame targets are already
    // debugged via content-process targets.
    // Do not attach the thread here, as it was already done by the
    // corresponding content-process target.
    return;
  }

  if (!targetFront.isTopLevel) {
    await actions.addTarget(targetFront);
    return;
  }

  if (isTargetSwitching) {
    // Simulate navigation actions when target switching.
    // The will-navigate event will be missed when using target switching,
    // however `navigate` corresponds more or less to the load event, so it
    // should still be received on the new target.
    actions.willNavigate({ url: targetFront.url });
  }

  // At this point, we expect the target and its thread to be attached.
  const { threadFront } = targetFront;
  if (!threadFront) {
    console.error("The thread for", targetFront, "isn't attached.");
    return;
  }

  targetFront.on("will-navigate", actions.willNavigate);
  targetFront.on("navigate", actions.navigated);

  await threadFront.reconfigure({
    observeAsmJS: true,
    pauseWorkersUntilAttach: true,
    skipBreakpoints: prefs.skipPausing,
    logEventBreakpoints: prefs.logEventBreakpoints,
  });

  // Retrieve possible event listener breakpoints
  actions.getEventListenerBreakpointTypes().catch(e => console.error(e));

  // Initialize the event breakpoints on the thread up front so that
  // they are active once attached.
  actions.addEventListenerBreakpoints([]).catch(e => console.error(e));

  await actions.connect(
    targetFront.url,
    threadFront.actor,
    targetFront.isWebExtension
  );

  await actions.addTarget(targetFront);
}

function onTargetDestroyed({ targetFront }) {
  if (targetFront.isTopLevel) {
    targetFront.off("will-navigate", actions.willNavigate);
    targetFront.off("navigate", actions.navigated);
  }
  actions.removeTarget(targetFront);
}

async function onSourceAvailable(sources) {
  const frontendSources = await Promise.all(
    sources
      .filter(source => {
        return !source.targetFront.isDestroyed();
      })
      .map(async source => {
        const threadFront = await source.targetFront.getFront("thread");
        const frontendSource = prepareSourcePayload(threadFront, source);
        return frontendSource;
      })
  );
  await actions.newGeneratedSources(frontendSources);
}

async function onBreakpointAvailable(breakpoints) {
  for (const resource of breakpoints) {
    const threadFront = await resource.targetFront.getFront("thread");
    if (resource.state == "paused") {
      const pause = await createPause(threadFront.actor, resource);
      actions.paused(pause);
      recordEvent("pause", { reason: resource.why.type });
    } else if (resource.state == "resumed") {
      actions.resumed(threadFront.actorID);
    }
  }
}

export { clientCommands };
