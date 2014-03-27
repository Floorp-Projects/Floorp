/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};


const { Class } = require("./heritage");
const { Observer, subscribe, unsubscribe, observe } = require("./observer");
const { isWeak, WeakReference } = require("./reference");
const method = require("../../method/core");

const unloadSubject = require('@loader/unload');
const addonUnloadTopic = "sdk:loader:destroy";



const uninstall = method("disposable/uninstall");
exports.uninstall = uninstall;


const shutdown = method("disposable/shutdown");
exports.shutdown = shutdown;

const disable = method("disposable/disable");
exports.disable = disable;

const upgrade = method("disposable/upgrade");
exports.upgrade = upgrade;

const downgrade = method("disposable/downgrade");
exports.downgrade = downgrade;

const unload = method("disposable/unload");
exports.unload = unload;

const dispose = method("disposable/dispose");
exports.dispose = dispose;
dispose.define(Object, object => object.dispose());


const setup = method("disposable/setup");
exports.setup = setup;
setup.define(Object, (object, ...args) => object.setup(...args));


// Set's up disposable instance.
const setupDisposable = disposable => {
  subscribe(disposable, addonUnloadTopic, isWeak(disposable));
};

// Tears down disposable instance.
const disposeDisposable = disposable => {
  unsubscribe(disposable, addonUnloadTopic);
};

// Base type that takes care of disposing it's instances on add-on unload.
// Also makes sure to remove unload listener if it's already being disposed.
const Disposable = Class({
  implements: [Observer],
  initialize: function(...args) {
    // First setup instance before initializing it's disposal. If instance
    // fails to initialize then there is no instance to be disposed at the
    // unload.
    setup(this, ...args);
    setupDisposable(this);
  },
  destroy: function(reason) {
    // Destroying disposable removes unload handler so that attempt to dispose
    // won't be made at unload & delegates to dispose.
    disposeDisposable(this);
    unload(this, reason);
  },
  setup: function() {
    // Implement your initialize logic here.
  },
  dispose: function() {
    // Implement your cleanup logic here.
  }
});
exports.Disposable = Disposable;

// Disposable instances observe add-on unload notifications in
// order to trigger `unload` on them.
observe.define(Disposable, (disposable, subject, topic, data) => {
  const isUnloadTopic = topic === addonUnloadTopic;
  const isUnloadSubject = subject.wrappedJSObject === unloadSubject;
  if (isUnloadTopic && isUnloadSubject) {
    unsubscribe(disposable, topic);
    unload(disposable);
  }
});

const unloaders = {
  destroy: dispose,
  uninstall: uninstall,
  shutdown: shutdown,
  disable: disable,
  upgrade: upgrade,
  downgrade: downgrade
}
const unloaded = new WeakMap();
unload.define(Disposable, (disposable, reason) => {
  if (!unloaded.get(disposable)) {
    unloaded.set(disposable, true);
    // Pick an unload handler associated with an unload
    // reason (falling back to destroy if not found) and
    // delegate unloading to it.
    const unload = unloaders[reason] || unloaders.destroy;
    unload(disposable);
  }
});


// If add-on is disabled munally, it's being upgraded, downgraded
// or uniststalled `dispose` is invoked to undo any changes that
// has being done by it in this session.
disable.define(Disposable, dispose);
downgrade.define(Disposable, dispose);
upgrade.define(Disposable, dispose);
uninstall.define(Disposable, dispose);

// If application is shut down no dispose is invoked as undo-ing
// changes made by instance is likely to just waste of resources &
// increase shutdown time. Although specefic components may choose
// to implement shutdown handler that does something better.
shutdown.define(Disposable, disposable => {});

