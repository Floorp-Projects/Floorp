/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Class } = require("./heritage");
const { Observer, subscribe, unsubscribe, observe } = require("./observer");
const { isWeak } = require("./reference");
const SDKWeakSet = require("../lang/weak-set");

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

// DisposablesUnloadObserver is the class which subscribe the
// Observer Service to be notified when the add-on loader is
// unloading to be able to dispose all the existent disposables.
const DisposablesUnloadObserver = Class({
  implements: [Observer],
  initialize: function(...args) {
    // Set of the non-weak disposables registered to be disposed.
    this.disposables = new Set();
    // Target of the weak disposables registered to be disposed
    // (and tracked on this target using the SDK weak-set module).
    this.weakDisposables = {};
  },
  subscribe(disposable) {
    if (isWeak(disposable)) {
      SDKWeakSet.add(this.weakDisposables, disposable);
    } else {
      this.disposables.add(disposable);
    }
  },
  unsubscribe(disposable) {
    if (isWeak(disposable)) {
      SDKWeakSet.remove(this.weakDisposables, disposable);
    } else {
      this.disposables.delete(disposable);
    }
  },
  tryUnloadDisposable(disposable) {
    try {
      if (disposable) {
        unload(disposable);
      }
    } catch(e) {
      console.error("Error unloading a",
                    isWeak(disposable) ? "weak disposable" : "disposable",
                    disposable, e);
    }
  },
  unloadAll() {
    // Remove all the subscribed disposables.
    for (let disposable of this.disposables) {
      this.tryUnloadDisposable(disposable);
    }

    this.disposables.clear();

    // Remove all the subscribed weak disposables.
    for (let disposable of SDKWeakSet.iterator(this.weakDisposables)) {
      this.tryUnloadDisposable(disposable);
    }

    SDKWeakSet.clear(this.weakDisposables);
  }
});
const disposablesUnloadObserver = new DisposablesUnloadObserver();

// The DisposablesUnloadObserver instance is the only object which subscribes
// the Observer Service directly, it observes add-on unload notifications in
// order to trigger `unload` on all its subscribed disposables.
observe.define(DisposablesUnloadObserver, (obj, subject, topic, data) => {
  const isUnloadTopic = topic === addonUnloadTopic;
  const isUnloadSubject = subject.wrappedJSObject === unloadSubject;
  if (isUnloadTopic && isUnloadSubject) {
    unsubscribe(disposablesUnloadObserver, addonUnloadTopic);
    disposablesUnloadObserver.unloadAll();
  }
});

subscribe(disposablesUnloadObserver, addonUnloadTopic, false);

// Set's up disposable instance.
const setupDisposable = disposable => {
  disposablesUnloadObserver.subscribe(disposable);
};
exports.setupDisposable = setupDisposable;

// Tears down disposable instance.
const disposeDisposable = disposable => {
  disposablesUnloadObserver.unsubscribe(disposable);
};
exports.disposeDisposable = disposeDisposable;

// Base type that takes care of disposing it's instances on add-on unload.
// Also makes sure to remove unload listener if it's already being disposed.
const Disposable = Class({
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

const unloaders = {
  destroy: dispose,
  uninstall: uninstall,
  shutdown: shutdown,
  disable: disable,
  upgrade: upgrade,
  downgrade: downgrade
};

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

// If add-on is disabled manually, it's being upgraded, downgraded
// or uninstalled `dispose` is invoked to undo any changes that
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
