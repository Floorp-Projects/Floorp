/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Registry indexing all specs and front modules
//
// All spec and front modules should be listed here
// in order to be referenced by any other spec or front module.
//
// TODO: For now we only register dynamically loaded specs and fronts here.
// See Bug 1399589 for supporting all specs and front modules.

// Declare in which spec module and front module a set of types are defined
const Types = [
  {
    types: ["pagestyle", "domstylerule"],
    spec: "devtools/shared/specs/styles",
    front: "devtools/shared/fronts/styles",
  },
  {
    types: ["highlighter", "customhighlighter"],
    spec: "devtools/shared/specs/highlighters",
    front: "devtools/shared/fronts/highlighters",
  },
  {
    types: ["grid", "layout"],
    spec: "devtools/shared/specs/layout",
    front: "devtools/shared/fronts/layout",
  },
  {
    types: ["longstring"],
    spec: "devtools/shared/specs/string",
    front: "devtools/shared/fronts/string",
  },
  {
    types: ["originalsource", "mediarule", "stylesheet", "stylesheets"],
    spec: "devtools/shared/specs/stylesheets",
    front: "devtools/shared/fronts/stylesheets",
  },
  {
    types: ["imageData", "domnode"],
    spec: "devtools/shared/specs/node",
    front: "devtools/shared/fronts/inspector",
  },
  {
    types: ["domwalker"],
    spec: "devtools/shared/specs/inspector",
    front: "devtools/shared/fronts/inspector",
  },
  {
    types: ["performance-recording"],
    spec: "devtools/shared/specs/performance-recording",
    front: "devtools/shared/fronts/performancec-recording",
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
  let modulePath = lazySpecs.get(type);
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
  let modulePath = lazyFronts.get(type);
  if (modulePath) {
    try {
      require(modulePath);
    } catch (e) {
      throw new Error(
        `Unable to load lazy front module '${modulePath}' for type '${type}'`);
    }
    lazyFronts.delete(type);
    return true;
  }
  return false;
}
exports.lazyLoadFront = lazyLoadFront;
