/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

let { emit, on, once, off, EVENT_TYPE_PATTERN } = require("./core");

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
  };
})();

function transform(input, f) {
  let output = {};

  // Since event listeners don't prevent `input` to be GC-ed we wanna presrve
  // it until `output` can be GC-ed. There for we add implicit reference which
  // is removed once `input` ends.
  refs(output).add(input);

  const next = data => receive(output, data);
  once(output, "start", () => start(input));
  on(input, "error", error => emit(output, "error", error));
  on(input, "end", function() {
    refs(output).delete(input);
    end(output);
  });
  on(input, "data", data => f(data, next));
  return output;
}

// High order event transformation function that takes `input` event channel
// and returns transformation containing only events on which `p` predicate
// returns `true`.
function filter(input, predicate) {
  return transform(input, function(data, next) {
    if (predicate(data))
      next(data);
  });
}
exports.filter = filter;

// High order function that takes `input` and returns input of it's values
// mapped via given `f` function.
const map = (input, f) => transform(input, (data, next) => next(f(data)));
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
  const error = e => emit(output, "error", e);
  function forward(input) {
    state.push(input);
    open = open + 1;
    on(input, "end", () => end(input));
    on(input, "error", error);
    on(input, "data", data => emit(output, "data", data));
  }

  // If `inputs` is an array treat it as a stream.
  if (Array.isArray(inputs)) {
    inputs.forEach(forward);
    end(inputs);
  }
  else {
    on(inputs, "end", () => end(inputs));
    on(inputs, "error", error);
    on(inputs, "data", forward);
  }

  return output;
}
exports.merge = merge;

const expand = (inputs, f) => merge(map(inputs, f));
exports.expand = expand;

const pipe = (from, to) => on(from, "*", emit.bind(emit, to));
exports.pipe = pipe;


// Shim signal APIs so other modules can be used as is.

const receive = (input, message) => {
  if (input[receive])
    input[receive](input, message);
  else
    emit(input, "data", message);

  input.value = message;
};
receive.toString = () => "@@receive";
exports.receive = receive;
exports.send = receive;

const end = input => {
  if (input[end])
    input[end](input);
  else
    emit(input, "end", input);
};
end.toString = () => "@@end";
exports.end = end;

const stop = input => {
  if (input[stop])
    input[stop](input);
  else
    emit(input, "stop", input);
};
stop.toString = () => "@@stop";
exports.stop = stop;

const start = input => {
  if (input[start])
    input[start](input);
  else
    emit(input, "start", input);
};
start.toString = () => "@@start";
exports.start = start;

const lift = (step, ...inputs) => {
  let args = null;
  let opened = inputs.length;
  let started = false;
  const output = {};
  const init = () => {
    args = [...inputs.map(input => input.value)];
    output.value = step(...args);
  };

  inputs.forEach((input, index) => {
    on(input, "data", data => {
      args[index] = data;
      receive(output, step(...args));
    });
    on(input, "end", () => {
      opened = opened - 1;
      if (opened <= 0)
        end(output);
    });
  });

  once(output, "start", () => {
    inputs.forEach(start);
    init();
  });

  init();

  return output;
};
exports.lift = lift;

const merges = inputs => {
  let opened = inputs.length;
  let output = { value: inputs[0].value };
  inputs.forEach((input, index) => {
    on(input, "data", data => receive(output, data));
    on(input, "end", () => {
      opened = opened - 1;
      if (opened <= 0)
        end(output);
    });
  });

  once(output, "start", () => {
    inputs.forEach(start);
    output.value = inputs[0].value;
  });

  return output;
};
exports.merges = merges;

const foldp = (step, initial, input) => {
  let output = map(input, x => step(output.value, x));
  output.value = initial;
  return output;
};
exports.foldp = foldp;

const keepIf = (p, base, input) => {
  let output = filter(input, p);
  output.value = base;
  return output;
};
exports.keepIf = keepIf;

function Input() {}
Input.start = input => emit(input, "start", input);
Input.prototype.start = Input.start;

Input.end = input => {
  emit(input, "end", input);
  stop(input);
};
Input.prototype[end] = Input.end;

exports.Input = Input;

const $source = "@@source";
const $outputs = "@@outputs";
exports.outputs = $outputs;

function Reactor(options={}) {
  const {onStep, onStart, onEnd} = options;
  if (onStep)
    this.onStep = onStep;
  if (onStart)
    this.onStart = onStart;
  if (onEnd)
    this.onEnd = onEnd;
}
Reactor.prototype.onStep = _ => void(0);
Reactor.prototype.onStart = _ => void(0);
Reactor.prototype.onEnd = _ => void(0);
Reactor.prototype.onNext = function(present, past) {
  this.value = present;
  this.onStep(present, past);
};
Reactor.prototype.run = function(input) {
  on(input, "data", message => this.onNext(message, input.value));
  on(input, "end", () => this.onEnd(input.value));
  start(input);
  this.value = input.value;
  this.onStart(input.value);
};
exports.Reactor = Reactor;

/**
 * Takes an object used as options with potential keys like 'onMessage',
 * used to be called `require('sdk/event/core').setListeners` on.
 * This strips all keys that would trigger a listener to be set.
 *
 * @params {Object} object
 * @return {Object}
 */

function stripListeners (object) {
  return Object.keys(object || {}).reduce((agg, key) => {
    if (!EVENT_TYPE_PATTERN.test(key))
      agg[key] = object[key];
    return agg;
  }, {});
}
exports.stripListeners = stripListeners;

const when = (target, type) => new Promise(resolve => {
  once(target, type, resolve);
});
exports.when = when;
