/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Registry indexing all specs and front modules
//
// All spec and front modules should be listed here
// in order to be referenced by any other spec or front module.

// Declare in which spec module and front module a set of types are defined.
// This array should be sorted by `spec` attribute.
const Types = exports.__TypesForTests = [
  {
    types: ["accessible", "accessiblewalker", "accessibility"],
    spec: "devtools/shared/specs/accessibility",
    front: "devtools/shared/fronts/accessibility",
  },
  {
    types: ["actorActor", "actorRegistry"],
    spec: "devtools/shared/specs/actor-registry",
    front: "devtools/shared/fronts/actor-registry",
  },
  {
    types: ["addons"],
    spec: "devtools/shared/specs/addon/addons",
    front: "devtools/shared/fronts/addon/addons",
  },
  {
    types: ["webExtension"],
    spec: "devtools/shared/specs/addon/webextension",
    front: null,
  },
  {
    types: ["webExtensionInspectedWindow"],
    spec: "devtools/shared/specs/addon/webextension-inspected-window",
    front: "devtools/shared/fronts/addon/webextension-inspected-window",
  },
  {
    types: ["animationplayer", "animations"],
    spec: "devtools/shared/specs/animation",
    front: "devtools/shared/fronts/animation",
  },
  {
    types: ["arraybuffer"],
    spec: "devtools/shared/specs/array-buffer",
    front: "devtools/shared/fronts/array-buffer",
  },
  {
    types: ["changes"],
    spec: "devtools/shared/specs/changes",
    front: "devtools/shared/fronts/changes",
  },
  {
    types: ["cssProperties"],
    spec: "devtools/shared/specs/css-properties",
    front: "devtools/shared/fronts/css-properties",
  },
  {
    types: ["device"],
    spec: "devtools/shared/specs/device",
    front: "devtools/shared/fronts/device",
  },
  {
    types: ["emulation"],
    spec: "devtools/shared/specs/emulation",
    front: "devtools/shared/fronts/emulation",
  },
  /* environment has old fashion client and no front */
  {
    types: ["environment"],
    spec: "devtools/shared/specs/environment",
    front: null,
  },
  /* frame has old fashion client and no front */
  {
    types: ["frame"],
    spec: "devtools/shared/specs/frame",
    front: null,
  },
  {
    types: ["framerate"],
    spec: "devtools/shared/specs/framerate",
    front: "devtools/shared/fronts/framerate",
  },
  /* heap snapshot has old fashion client and no front */
  {
    types: ["heapSnapshotFile"],
    spec: "devtools/shared/specs/heap-snapshot-file",
    front: null,
  },
  {
    types: ["highlighter", "customhighlighter"],
    spec: "devtools/shared/specs/highlighters",
    front: "devtools/shared/fronts/highlighters",
  },
  {
    types: ["domnodelist", "domwalker", "inspector"],
    spec: "devtools/shared/specs/inspector",
    front: "devtools/shared/fronts/inspector",
  },
  {
    types: ["flexbox", "grid", "layout"],
    spec: "devtools/shared/specs/layout",
    front: "devtools/shared/fronts/layout",
  },
  {
    types: ["memory"],
    spec: "devtools/shared/specs/memory",
    front: "devtools/shared/fronts/memory",
  },
  {
    types: ["netEvent"],
    spec: "devtools/shared/specs/network-event",
    front: null,
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
    front: "devtools/shared/fronts/node",
  },
  {
    types: ["obj", "object.descriptor"],
    spec: "devtools/shared/specs/object",
    front: null,
  },
  {
    types: ["perf"],
    spec: "devtools/shared/specs/perf",
    front: "devtools/shared/fronts/perf",
  },
  {
    types: ["performance"],
    spec: "devtools/shared/specs/performance",
    front: "devtools/shared/fronts/performance",
  },
  {
    types: ["performance-recording"],
    spec: "devtools/shared/specs/performance-recording",
    front: "devtools/shared/fronts/performance-recording",
  },
  {
    types: ["preference"],
    spec: "devtools/shared/specs/preference",
    front: "devtools/shared/fronts/preference",
  },
  {
    types: ["promises"],
    spec: "devtools/shared/specs/promises",
    front: "devtools/shared/fronts/promises",
  },
  {
    types: ["propertyIterator"],
    spec: "devtools/shared/specs/property-iterator",
    front: null,
  },
  {
    types: ["reflow"],
    spec: "devtools/shared/specs/reflow",
    front: "devtools/shared/fronts/reflow",
  },
  {
    types: ["screenshot"],
    spec: "devtools/shared/specs/screenshot",
    front: "devtools/shared/fronts/screenshot",
  },
  {
    types: ["source"],
    spec: "devtools/shared/specs/source",
    front: "devtools/shared/fronts/source",
  },
  {
    types: ["cookies", "localStorage", "sessionStorage", "Cache", "indexedDB", "storage"],
    spec: "devtools/shared/specs/storage",
    front: "devtools/shared/fronts/storage",
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
    front: "devtools/shared/fronts/string",
  },
  {
    types: ["pagestyle", "domstylerule"],
    spec: "devtools/shared/specs/styles",
    front: "devtools/shared/fronts/styles",
  },
  {
    types: ["mediarule", "stylesheet", "stylesheets"],
    spec: "devtools/shared/specs/stylesheets",
    front: "devtools/shared/fronts/stylesheets",
  },
  {
    types: ["symbol"],
    spec: "devtools/shared/specs/symbol",
    front: null,
  },
  {
    types: ["symbolIterator"],
    spec: "devtools/shared/specs/symbol-iterator",
    front: null,
  },
  {
    types: ["addonTarget"],
    spec: "devtools/shared/specs/targets/addon",
    front: "devtools/shared/fronts/targets/addon",
  },
  {
    types: ["browsingContextTarget"],
    spec: "devtools/shared/specs/targets/browsing-context",
    front: "devtools/shared/fronts/targets/browsing-context",
  },
  {
    types: ["chromeWindowTarget"],
    spec: "devtools/shared/specs/targets/chrome-window",
    front: null,
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
    front: "devtools/shared/fronts/targets/worker",
  },
  /* Thread has an old fashion client and no front */
  {
    types: ["context"],
    spec: "devtools/shared/specs/thread",
    front: null,
  },
  {
    types: ["console"],
    spec: "devtools/shared/specs/webconsole",
    front: "devtools/shared/fronts/webconsole",
  },
  {
    types: ["pushSubscription"],
    spec: "devtools/shared/specs/worker/push-subscription",
    front: "devtools/shared/fronts/worker/push-subscription",
  },
  {
    types: ["serviceWorker"],
    spec: "devtools/shared/specs/worker/service-worker",
    front: "devtools/shared/fronts/worker/service-worker",
  },
  {
    types: ["serviceWorkerRegistration"],
    spec: "devtools/shared/specs/worker/service-worker-registration",
    front: "devtools/shared/fronts/worker/service-worker-registration",
  },
];

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
        `Unable to load lazy spec module '${modulePath}' for type '${type}'`);
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
        Error: ${e}`);
    }
    lazyFronts.delete(type);
    return true;
  }
  return false;
}
exports.lazyLoadFront = lazyLoadFront;
