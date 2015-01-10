"use strict";

const shared = require("toolkit/require");
const { Item, Separator, Menu, Contexts, Readers } = shared.require("sdk/context-menu/core");
const { setupDisposable, disposeDisposable, Disposable } = require("sdk/core/disposable")
const { Class } = require("sdk/core/heritage")

const makeDisposable = Type => Class({
  extends: Type,
  implements: [Disposable],
  initialize: Type.prototype.initialize,
  setup(...params) {
    Type.prototype.setup.call(this, ...params);
    setupDisposable(this);
  },
  dispose(...params) {
    disposeDisposable(this);
    Type.prototype.dispose.call(this, ...params);
  }
});

exports.Separator = Separator;
exports.Contexts = Contexts;
exports.Readers = Readers;

// Subclass Item & Menu shared classes so their items
// will be unloaded when add-on is unloaded.
exports.Item = makeDisposable(Item);
exports.Menu = makeDisposable(Menu);
