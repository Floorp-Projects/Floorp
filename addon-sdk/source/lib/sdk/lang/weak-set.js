"use strict";

const { Cu } = require("chrome");

function makeGetterFor(Type) {
  let cache = new WeakMap();

  return function getFor(target) {
    if (!cache.has(target))
      cache.set(target, new Type());

    return cache.get(target);
  }
}

let getLookupFor = makeGetterFor(WeakMap);
let getRefsFor = makeGetterFor(Set);

function add(target, value) {
  if (has(target, value))
    return;

  getLookupFor(target).set(value, true);
  getRefsFor(target).add(Cu.getWeakReference(value));
}
exports.add = add;

function remove(target, value) {
  getLookupFor(target).delete(value);
}
exports.remove = remove;

function has(target, value) {
  return getLookupFor(target).has(value);
}
exports.has = has;

function clear(target) {
  getLookupFor(target).clear();
  getRefsFor(target).clear();
}
exports.clear = clear;

function iterator(target) {
  let refs = getRefsFor(target);

  for (let ref of refs) {
    let value = ref.get();

    if (has(target, value))
      yield value;
    else
      refs.delete(ref);
  }
}
exports.iterator = iterator;
