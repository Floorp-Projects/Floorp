/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { setupCommands, clientCommands } from "./firefox/commands";
import { setupCreate, createPause } from "./firefox/create";
import { features } from "../utils/prefs";

import { recordEvent } from "../utils/telemetry";
import sourceQueue from "../utils/source-queue";
import { getContext } from "../selectors";

let actions;
let commands;
let targetCommand;
let resourceCommand;

export async function onConnect(_commands, _resourceCommand, _actions, store) {
  actions = _actions;
  commands = _commands;
  targetCommand = _commands.targetCommand;
  resourceCommand = _resourceCommand;

  setupCommands(commands);
  setupCreate({ store });

  sourceQueue.initialize(actions);

  const { descriptorFront } = commands;
  const { targetFront } = targetCommand;

  // For tab, browser and webextension toolboxes, we want to enable watching for
  // worker targets as soon as the debugger is opened.
  // And also for service workers, if the related experimental feature is enabled
  if (
    descriptorFront.isTabDescriptor ||
    descriptorFront.isWebExtensionDescriptor ||
    descriptorFront.isBrowserProcessDescriptor
  ) {
    targetCommand.listenForWorkers = true;
    if (descriptorFront.isLocalTab && features.windowlessServiceWorkers) {
      targetCommand.listenForServiceWorkers = true;
      targetCommand.destroyServiceWorkersOnNavigation = true;
    }
    await targetCommand.startListening();
  }

  const options = {
    // `pauseWorkersUntilAttach` is one option set when the debugger panel is opened rather that from the toolbox.
    // The reason is to support early breakpoints in workers, which will force the workers to pause
    // and later on (when TargetMixin.attachThread is called) resume worker execution, after passing the breakpoints.
    // We only observe workers when the debugger panel is opened (see the few lines before and listenForWorkers = true).
    // So if we were passing `pauseWorkersUntilAttach=true` from the toolbox code, workers would freeze as we would not watch
    // for their targets and not resume them.
    pauseWorkersUntilAttach: true,

    // Bug 1719615 - Immediately turn on WASM debugging when the debugger opens.
    // We avoid enabling that as soon as DevTools open as WASM generates different kind of machine code
    // with debugging instruction which significantly increase the memory usage.
    observeWasm: true,
  };
  await commands.threadConfigurationCommand.updateConfiguration(options);

  // Select the top level target by default
  await actions.selectThread(
    getContext(store.getState()),
    targetFront.threadFront.actor
  );

  await targetCommand.watchTargets({
    types: targetCommand.ALL_TYPES,
    onAvailable: onTargetAvailable,
    onDestroyed: onTargetDestroyed,
  });

  // Use independant listeners for SOURCE and THREAD_STATE in order to ease
  // doing batching and notify about a set of SOURCE's in one redux action.
  await resourceCommand.watchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable: onSourceAvailable,
  });
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: onThreadStateAvailable,
  });

  await resourceCommand.watchResources([resourceCommand.TYPES.ERROR_MESSAGE], {
    onAvailable: actions.addExceptionFromResources,
  });
  await resourceCommand.watchResources([resourceCommand.TYPES.DOCUMENT_EVENT], {
    onAvailable: onDocumentEventAvailable,
    // we only care about future events for DOCUMENT_EVENT
    ignoreExistingResources: true,
  });
}

export function onDisconnect() {
  targetCommand.unwatchTargets({
    types: targetCommand.ALL_TYPES,
    onAvailable: onTargetAvailable,
    onDestroyed: onTargetDestroyed,
  });
  resourceCommand.unwatchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable: onSourceAvailable,
  });
  resourceCommand.unwatchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: onThreadStateAvailable,
  });
  resourceCommand.unwatchResources([resourceCommand.TYPES.ERROR_MESSAGE], {
    onAvailable: actions.addExceptionFromResources,
  });
  resourceCommand.unwatchResources([resourceCommand.TYPES.DOCUMENT_EVENT], {
    onAvailable: onDocumentEventAvailable,
  });
  sourceQueue.clear();
}

async function onTargetAvailable({ targetFront, isTargetSwitching }) {
  const isBrowserToolbox = commands.descriptorFront.isBrowserProcessDescriptor;
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

  // At this point, we expect the target and its thread to be attached.
  const { threadFront } = targetFront;
  if (!threadFront) {
    console.error("The thread for", targetFront, "isn't attached.");
    return;
  }

  // Retrieve possible event listener breakpoints
  actions.getEventListenerBreakpointTypes().catch(e => console.error(e));

  // Initialize the event breakpoints on the thread up front so that
  // they are active once attached.
  actions.addEventListenerBreakpoints([]).catch(e => console.error(e));

  await actions.addTarget(targetFront);
}

function onTargetDestroyed({ targetFront }) {
  actions.removeTarget(targetFront);
}

async function onSourceAvailable(sources) {
  await actions.newGeneratedSources(sources);
}

async function onThreadStateAvailable(resources) {
  for (const resource of resources) {
    if (resource.targetFront.isDestroyed()) {
      continue;
    }
    const threadFront = await resource.targetFront.getFront("thread");
    if (resource.state == "paused") {
      const pause = await createPause(threadFront.actor, resource);
      await actions.paused(pause);
      recordEvent("pause", { reason: resource.why.type });
    } else if (resource.state == "resumed") {
      await actions.resumed(threadFront.actorID);
    }
  }
}

function onDocumentEventAvailable(events) {
  for (const event of events) {
    // Only consider top level document, and ignore remote iframes top document
    if (!event.targetFront.isTopLevel) continue;

    if (event.name == "will-navigate") {
      actions.willNavigate({ url: event.newURI });
    } else if (event.name == "dom-complete") {
      actions.navigated();
    }
  }
}

export { clientCommands };
