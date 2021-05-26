/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Registry indexing all specs and front modules
//
// All spec and front modules should be listed here
// in order to be referenced by any other spec or front module.

// Declare in which spec module and front module a set of types are defined.
// This array should be sorted by `spec` attribute, and this is verified in the
// test devtools/shared/protocol/tests/xpcshell/test_protocol_index.js
const Types = (exports.__TypesForTests = [
  {
    types: [
      "accessible",
      "accessiblewalker",
      "accessibility",
      "parentaccessibility",
    ],
    spec: "devtools/shared/specs/accessibility",
    front: "devtools/client/fronts/accessibility",
  },
  {
    types: ["addons"],
    spec: "devtools/shared/specs/addon/addons",
    front: "devtools/client/fronts/addon/addons",
  },
  {
    types: ["webExtensionInspectedWindow"],
    spec: "devtools/shared/specs/addon/webextension-inspected-window",
    front: "devtools/client/fronts/addon/webextension-inspected-window",
  },
  {
    types: ["animationplayer", "animations"],
    spec: "devtools/shared/specs/animation",
    front: "devtools/client/fronts/animation",
  },
  {
    types: ["arraybuffer"],
    spec: "devtools/shared/specs/array-buffer",
    front: "devtools/client/fronts/array-buffer",
  },
  {
    types: ["breakpoint-list"],
    spec: "devtools/shared/specs/breakpoint-list",
    front: "devtools/client/fronts/breakpoint-list",
  },
  {
    types: ["changes"],
    spec: "devtools/shared/specs/changes",
    front: "devtools/client/fronts/changes",
  },
  {
    types: ["compatibility"],
    spec: "devtools/shared/specs/compatibility",
    front: "devtools/client/fronts/compatibility",
  },
  {
    types: ["cssProperties"],
    spec: "devtools/shared/specs/css-properties",
    front: "devtools/client/fronts/css-properties",
  },
  {
    types: ["processDescriptor"],
    spec: "devtools/shared/specs/descriptors/process",
    front: "devtools/client/fronts/descriptors/process",
  },
  {
    types: ["tabDescriptor"],
    spec: "devtools/shared/specs/descriptors/tab",
    front: "devtools/client/fronts/descriptors/tab",
  },
  {
    types: ["webExtensionDescriptor"],
    spec: "devtools/shared/specs/descriptors/webextension",
    front: "devtools/client/fronts/descriptors/webextension",
  },
  {
    types: ["workerDescriptor"],
    spec: "devtools/shared/specs/descriptors/worker",
    front: "devtools/client/fronts/descriptors/worker",
  },
  {
    types: ["device"],
    spec: "devtools/shared/specs/device",
    front: "devtools/client/fronts/device",
  },
  {
    types: ["environment"],
    spec: "devtools/shared/specs/environment",
    front: null,
  },
  {
    types: ["eventSource"],
    spec: "devtools/shared/specs/eventsource",
    front: "devtools/client/fronts/eventsource",
  },
  {
    types: ["frame"],
    spec: "devtools/shared/specs/frame",
    front: "devtools/client/fronts/frame",
  },
  {
    types: ["framerate"],
    spec: "devtools/shared/specs/framerate",
    front: "devtools/client/fronts/framerate",
  },
  /* heap snapshot has old fashion client and no front */
  {
    types: ["heapSnapshotFile"],
    spec: "devtools/shared/specs/heap-snapshot-file",
    front: null,
  },
  {
    types: ["customhighlighter"],
    spec: "devtools/shared/specs/highlighters",
    front: "devtools/client/fronts/highlighters",
  },
  {
    types: ["inspector"],
    spec: "devtools/shared/specs/inspector",
    front: "devtools/client/fronts/inspector",
  },
  {
    types: ["flexbox", "grid", "layout"],
    spec: "devtools/shared/specs/layout",
    front: "devtools/client/fronts/layout",
  },
  {
    types: ["manifest"],
    spec: "devtools/shared/specs/manifest",
    front: "devtools/client/fronts/manifest",
  },
  {
    types: ["mediarule"],
    spec: "devtools/shared/specs/media-rule",
    front: "devtools/client/fronts/media-rule",
  },
  {
    types: ["memory"],
    spec: "devtools/shared/specs/memory",
    front: "devtools/client/fronts/memory",
  },
  {
    types: ["networkContent"],
    spec: "devtools/shared/specs/network-content",
    front: "devtools/client/fronts/network-content",
  },
  {
    types: ["netEvent"],
    spec: "devtools/shared/specs/network-event",
    front: null,
  },
  {
    types: ["networkParent"],
    spec: "devtools/shared/specs/network-parent",
    front: "devtools/client/fronts/network-parent",
  },
  /* imageData isn't an actor but just a DictType */
  {
    types: ["imageData", "disconnectedNode", "disconnectedNodeArray"],
    spec: "devtools/shared/specs/node",
    front: null,
  },
  {
    types: ["domnode", "domnodelist"],
    spec: "devtools/shared/specs/node",
    front: "devtools/client/fronts/node",
  },
  {
    types: ["obj", "object.descriptor"],
    spec: "devtools/shared/specs/object",
    front: null,
  },
  {
    types: ["pagestyle"],
    spec: "devtools/shared/specs/page-style",
    front: "devtools/client/fronts/page-style",
  },
  {
    types: ["perf"],
    spec: "devtools/shared/specs/perf",
    front: "devtools/client/fronts/perf",
  },
  {
    types: ["performance"],
    spec: "devtools/shared/specs/performance",
    front: "devtools/client/fronts/performance",
  },
  {
    types: ["performance-recording"],
    spec: "devtools/shared/specs/performance-recording",
    front: "devtools/client/fronts/performance-recording",
  },
  {
    types: ["preference"],
    spec: "devtools/shared/specs/preference",
    front: "devtools/client/fronts/preference",
  },
  {
    types: ["privatePropertiesIterator"],
    spec: "devtools/shared/specs/private-properties-iterator",
    front: "devtools/client/fronts/private-properties-iterator",
  },
  {
    types: ["propertyIterator"],
    spec: "devtools/shared/specs/property-iterator",
    front: "devtools/client/fronts/property-iterator",
  },
  {
    types: ["reflow"],
    spec: "devtools/shared/specs/reflow",
    front: "devtools/client/fronts/reflow",
  },
  {
    types: ["responsive"],
    spec: "devtools/shared/specs/responsive",
    front: "devtools/client/fronts/responsive",
  },
  {
    types: ["root"],
    spec: "devtools/shared/specs/root",
    front: "devtools/client/fronts/root",
  },
  {
    types: ["screenshot"],
    spec: "devtools/shared/specs/screenshot",
    front: "devtools/client/fronts/screenshot",
  },
  {
    types: ["screenshot-content"],
    spec: "devtools/shared/specs/screenshot-content",
    front: "devtools/client/fronts/screenshot-content",
  },
  {
    types: ["source"],
    spec: "devtools/shared/specs/source",
    front: "devtools/client/fronts/source",
  },
  {
    types: [
      "cookies",
      "localStorage",
      "sessionStorage",
      "Cache",
      "indexedDB",
      "storage",
    ],
    spec: "devtools/shared/specs/storage",
    front: "devtools/client/fronts/storage",
  },
  /* longstring is special, it has a wrapper type. See its spec module */
  {
    types: ["longstring"],
    spec: "devtools/shared/specs/string",
    front: null,
  },
  {
    types: ["longstractor"],
    spec: "devtools/shared/specs/string",
    front: "devtools/client/fronts/string",
  },
  {
    types: ["domstylerule"],
    spec: "devtools/shared/specs/style-rule",
    front: "devtools/client/fronts/style-rule",
  },
  {
    types: ["stylesheet"],
    spec: "devtools/shared/specs/style-sheet",
    front: "devtools/client/fronts/style-sheet",
  },
  {
    types: ["stylesheets"],
    spec: "devtools/shared/specs/style-sheets",
    front: "devtools/client/fronts/style-sheets",
  },
  {
    types: ["symbol"],
    spec: "devtools/shared/specs/symbol",
    front: null,
  },
  {
    types: ["symbolIterator"],
    spec: "devtools/shared/specs/symbol-iterator",
    front: "devtools/client/fronts/symbol-iterator",
  },
  {
    types: ["target-configuration"],
    spec: "devtools/shared/specs/target-configuration",
    front: "devtools/client/fronts/target-configuration",
  },
  {
    types: ["browsingContextTarget"],
    spec: "devtools/shared/specs/targets/browsing-context",
    front: "devtools/client/fronts/targets/browsing-context",
  },
  {
    types: ["contentProcessTarget"],
    spec: "devtools/shared/specs/targets/content-process",
    front: null,
  },
  {
    types: ["frameTarget"],
    spec: "devtools/shared/specs/targets/frame",
    front: null,
  },
  {
    types: ["parentProcessTarget"],
    spec: "devtools/shared/specs/targets/parent-process",
    front: null,
  },
  {
    types: ["webExtensionTarget"],
    spec: "devtools/shared/specs/targets/webextension",
    front: null,
  },
  {
    types: ["workerTarget"],
    spec: "devtools/shared/specs/targets/worker",
    front: "devtools/client/fronts/targets/worker",
  },
  {
    types: ["thread"],
    spec: "devtools/shared/specs/thread",
    front: "devtools/client/fronts/thread",
  },
  {
    types: ["thread-configuration"],
    spec: "devtools/shared/specs/thread-configuration",
    front: "devtools/client/fronts/thread-configuration",
  },
  {
    types: ["domwalker"],
    spec: "devtools/shared/specs/walker",
    front: "devtools/client/fronts/walker",
  },
  {
    types: ["watcher"],
    spec: "devtools/shared/specs/watcher",
    front: "devtools/client/fronts/watcher",
  },
  {
    types: ["console"],
    spec: "devtools/shared/specs/webconsole",
    front: "devtools/client/fronts/webconsole",
  },
  {
    types: ["webSocket"],
    spec: "devtools/shared/specs/websocket",
    front: "devtools/client/fronts/websocket",
  },
  {
    types: ["pushSubscription"],
    spec: "devtools/shared/specs/worker/push-subscription",
    front: "devtools/client/fronts/worker/push-subscription",
  },
  {
    types: ["serviceWorker"],
    spec: "devtools/shared/specs/worker/service-worker",
    front: "devtools/client/fronts/worker/service-worker",
  },
  {
    types: ["serviceWorkerRegistration"],
    spec: "devtools/shared/specs/worker/service-worker-registration",
    front: "devtools/client/fronts/worker/service-worker-registration",
  },
]);

const lazySpecs = new Map();
const lazyFronts = new Map();

// Convert the human readable `Types` list into efficient maps
Types.forEach(item => {
  item.types.forEach(type => {
    lazySpecs.set(type, item.spec);
    lazyFronts.set(type, item.front);
  });
});

/**
 * Try lazy loading spec module for the given type.
 *
 * @param [string] type
 *    Type name
 *
 * @returns true, if it matched a lazy loaded type and tried to load it.
 */
function lazyLoadSpec(type) {
  const modulePath = lazySpecs.get(type);
  if (modulePath) {
    try {
      require(modulePath);
    } catch (e) {
      throw new Error(
        `Unable to load lazy spec module '${modulePath}' for type '${type}'.
        Error: ${e}`
      );
    }
    lazySpecs.delete(type);
    return true;
  }
  return false;
}
exports.lazyLoadSpec = lazyLoadSpec;

/**
 * Try lazy loading front module for the given type.
 *
 * @param [string] type
 *    Type name
 *
 * @returns true, if it matched a lazy loaded type and tried to load it.
 */
function lazyLoadFront(type) {
  const modulePath = lazyFronts.get(type);
  if (modulePath) {
    try {
      require(modulePath);
    } catch (e) {
      throw new Error(
        `Unable to load lazy front module '${modulePath}' for type '${type}'.
        Error: ${e}`
      );
    }
    lazyFronts.delete(type);
    return true;
  }
  return false;
}
exports.lazyLoadFront = lazyLoadFront;
