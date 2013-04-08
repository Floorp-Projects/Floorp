/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

let { emit, on, off } = require("./core");

// This module provides set of high order function for working with event
// streams (streams in a NodeJS style that dispatch data, end and error
// events).

// Function takes a `target` object and returns set of implicit references
// (non property references) it keeps. This basically allows defining
// references between objects without storing the explicitly. See transform for
// more details.
let refs = (function() {
  let refSets = new WeakMap();
  return function refs(target) {
    if (!refSets.has(target)) refSets.set(target, new Set());
    return refSets.get(target);
  }
})();

function transform(f, input) {
  let output = {};

  // Since event listeners don't prevent `input` to be GC-ed we wanna presrve
  // it until `output` can be GC-ed. There for we add implicit reference which
  // is removed once `input` ends.
  refs(output).add(input);

  function next(data) emit(output, "data", data);
  on(input, "error", function(error) emit(output, "error", error));
  on(input, "end", function() {
    refs(output).delete(input);
    emit(output, "end");
  });
  on(input, "data", function(data) f(data, next));
  return output;
}

// High order event transformation function that takes `input` event channel
// and returns transformation containing only events on which `p` predicate
// returns `true`.
function filter(predicate, input) {
  return transform(function(data, next) {
    if (predicate(data)) next(data)
  }, input);
}
exports.filter = filter;

// High order function that takes `input` and returns input of it's values
// mapped via given `f` function.
function map(f, input) transform(function(data, next) next(f(data)), input)
exports.map = map;

// High order function that takes `input` stream of streams and merges them
// into single event stream. Like flatten but time based rather than order
// based.
function merge(inputs) {
  let output = {};
  let open = 1;
  let state = [];
  output.state = state;
  refs(output).add(inputs);

  function end(input) {
    open = open - 1;
    refs(output).delete(input);
    if (open === 0) emit(output, "end");
  }
  function error(e) emit(output, "error", e);
  function forward(input) {
    state.push(input);
    open = open + 1;
    on(input, "end", function() end(input));
    on(input, "error", error);
    on(input, "data", function(data) emit(output, "data", data));
  }

  // If `inputs` is an array treat it as a stream.
  if (Array.isArray(inputs)) {
    inputs.forEach(forward)
    end(inputs)
  }
  else {
    on(inputs, "end", function() end(inputs));
    on(inputs, "error", error);
    on(inputs, "data", forward);
  }

  return output;
}
exports.merge = merge;

function expand(f, inputs) merge(map(f, inputs))
exports.expand = expand;
