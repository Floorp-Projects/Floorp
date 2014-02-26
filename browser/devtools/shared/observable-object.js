/* this source code form is subject to the terms of the mozilla public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ObservableObject
 *
 * An observable object is a JSON-like object that throws
 * events when its direct properties or properties of any
 * contained objects, are getting accessed or set.
 *
 * Inherits from EventEmitter.
 *
 * Properties:
 * ⬩ object: JSON-like object
 *
 * Events:
 * ⬩ "get" / path (array of property names)
 * ⬩ "set" / path / new value
 *
 * Example:
 *
 *   let emitter = new ObservableObject({ x: { y: [10] } });
 *   emitter.on("set", console.log);
 *   emitter.on("get", console.log);
 *   let obj = emitter.object;
 *   obj.x.y[0] = 50;
 *
 */

"use strict";

const EventEmitter = require("devtools/toolkit/event-emitter");

function ObservableObject(object = {}) {
  EventEmitter.decorate(this);
  let handler = new Handler(this);
  this.object = new Proxy(object, handler);
  handler._wrappers.set(this.object, object);
  handler._paths.set(object, []);
}

module.exports = ObservableObject;

function isObject(x) {
  if (typeof x === "object")
    return x !== null;
  return typeof x === "function";
}

function Handler(emitter) {
  this._emitter = emitter;
  this._wrappers = new WeakMap();
  this._values = new WeakMap();
  this._paths = new WeakMap();
}

Handler.prototype = {
  wrap: function(target, key, value) {
    let path;
    if (!isObject(value)) {
      path = this._paths.get(target).concat(key);
    } else if (this._wrappers.has(value)) {
      path = this._paths.get(value);
    } else if (this._paths.has(value)) {
      path = this._paths.get(value);
      value = this._values.get(value);
    } else {
      path = this._paths.get(target).concat(key);
      this._paths.set(value, path);
      let wrapper = new Proxy(value, this);
      this._wrappers.set(wrapper, value);
      this._values.set(value, wrapper);
      value = wrapper;
    }
    return [value, path];
  },
  unwrap: function(target, key, value) {
    if (!isObject(value) || !this._wrappers.has(value)) {
      return [value, this._paths.get(target).concat(key)];
    }
    return [this._wrappers.get(value), this._paths.get(target).concat(key)];
  },
  get: function(target, key) {
    let value = target[key];
    let [wrapped, path] = this.wrap(target, key, value);
    this._emitter.emit("get", path, value);
    return wrapped;
  },
  set: function(target, key, value) {
    let [wrapped, path] = this.unwrap(target, key, value);
    target[key] = value;
    this._emitter.emit("set", path, value);
  },
  getOwnPropertyDescriptor: function(target, key) {
    let desc = Object.getOwnPropertyDescriptor(target, key);
    if (desc) {
      if ("value" in desc) {
        let [wrapped, path] = this.wrap(target, key, desc.value);
        desc.value = wrapped;
        this._emitter.emit("get", path, desc.value);
      } else {
        if ("get" in desc) {
          [desc.get] = this.wrap(target, "get "+key, desc.get);
        }
        if ("set" in desc) {
          [desc.set] = this.wrap(target, "set "+key, desc.set);
        }
      }
    }
    return desc;
  },
  defineProperty: function(target, key, desc) {
    if ("value" in desc) {
      let [unwrapped, path] = this.unwrap(target, key, desc.value);
      desc.value = unwrapped;
      Object.defineProperty(target, key, desc);
      this._emitter.emit("set", path, desc.value);
    } else {
      if ("get" in desc) {
        [desc.get] = this.unwrap(target, "get "+key, desc.get);
      }
      if ("set" in desc) {
        [desc.set] = this.unwrap(target, "set "+key, desc.set);
      }
      Object.defineProperty(target, key, desc);
    }
  }
};
