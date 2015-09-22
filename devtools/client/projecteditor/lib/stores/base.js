/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cc, Ci, Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { EventTarget } = require("sdk/event/target");
const { emit } = require("sdk/event/core");
const promise = require("promise");

/**
 * A Store object maintains a collection of Resource objects stored in a tree.
 *
 * The Store class should not be instantiated directly.  Instead, you should
 * use a class extending it - right now this is only a LocalStore.
 *
 * Events:
 * This object emits the 'resource-added' and 'resource-removed' events.
 */
var Store = Class({
  extends: EventTarget,

  /**
   * Should be called during initialize() of a subclass.
   */
  initStore: function() {
    this.resources = new Map();
  },

  refresh: function() {
    return promise.resolve();
  },

  /**
   * Return a sorted Array of all Resources in the Store
   */
  allResources: function() {
    var resources = [];
    function addResource(resource) {
      resources.push(resource);
      resource.childrenSorted.forEach(addResource);
    }
    addResource(this.root);
    return resources;
  },

  notifyAdd: function(resource) {
    emit(this, "resource-added", resource);
  },

  notifyRemove: function(resource) {
    emit(this, "resource-removed", resource);
  }
});

exports.Store = Store;
