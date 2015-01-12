/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { EventEmitter } = require('../deprecated/events');
const unload = require('../system/unload');

const Registry = EventEmitter.compose({
  _registry: null,
  _constructor: null,
  constructor: function Registry(constructor) {
    this._registry = [];
    this._constructor = constructor;
    this.on('error', this._onError = this._onError.bind(this));
    unload.ensure(this, "_destructor");
  },
  _destructor: function _destructor() {
    let _registry = this._registry.slice(0);
    for (let instance of _registry)
      this._emit('remove', instance);
    this._registry.splice(0);
  },
  _onError: function _onError(e) {
    if (!this._listeners('error').length)
      console.error(e);
  },
  has: function has(instance) {
    let _registry = this._registry;
    return (
      (0 <= _registry.indexOf(instance)) ||
      (instance && instance._public && 0 <= _registry.indexOf(instance._public))
    );
  },
  add: function add(instance) {
    let { _constructor, _registry } = this; 
    if (!(instance instanceof _constructor))
      instance = new _constructor(instance);
    if (0 > _registry.indexOf(instance)) {
      _registry.push(instance);
      this._emit('add', instance);
    }
    return instance;
  },
  remove: function remove(instance) {
    let _registry = this._registry;
    let index = _registry.indexOf(instance)
    if (0 <= index) {
      this._emit('remove', instance);
      _registry.splice(index, 1);
    }
  }
});
exports.Registry = Registry;

