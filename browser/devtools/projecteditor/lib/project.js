/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { EventTarget } = require("sdk/event/target");
const { emit } = require("sdk/event/core");
const { scope, on, forget } = require("projecteditor/helpers/event");
const prefs = require("sdk/preferences/service");
const { LocalStore } = require("projecteditor/stores/local");
const { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
const promise = require("projecteditor/helpers/promise");
const { TextEncoder, TextDecoder } = require('sdk/io/buffer');
const url = require('sdk/url');

const gDecoder = new TextDecoder();
const gEncoder = new TextEncoder();

/**
 * A Project keeps track of the opened folders using LocalStore
 * objects.  Resources are generally requested from the project,
 * even though the Store is actually keeping track of them.
 *
 *
 * This object emits the following events:
 *   - "refresh-complete": After all stores have been refreshed from disk.
 *   - "store-added": When a store has been added to the project.
 *   - "store-removed": When a store has been removed from the project.
 *   - "resource-added": When a resource has been added to one of the stores.
 *   - "resource-removed": When a resource has been removed from one of the stores.
 */
var Project = Class({
  extends: EventTarget,

  /**
   * Intialize the Project.
   *
   * @param Object options
   *               Options to be passed into Project.load function
   */
  initialize: function(options) {
    this.localStores = new Map();

    this.load(options);
  },

  destroy: function() {
    // We are removing the store because the project never gets persisted.
    // There may need to be separate destroy functionality that doesn't remove
    // from project if this is saved to DB.
    this.removeAllStores();
  },

  toString: function() {
    return "[Project] " + this.name;
  },

  /**
   * Load a project given metadata about it.
   *
   * @param Object options
   *               Information about the project, containing:
   *                id: An ID (currently unused, but could be used for saving)
   *                name: The display name of the project
   *                directories: An array of path strings to load
   */
  load: function(options) {
    this.id = options.id;
    this.name = options.name || "Untitled";

    let paths = new Set(options.directories.map(name => OS.Path.normalize(name)));

    for (let [path, store] of this.localStores) {
      if (!paths.has(path)) {
        this.removePath(path);
      }
    }

    for (let path of paths) {
      this.addPath(path);
    }
  },

  /**
   * Refresh all project stores from disk
   *
   * @returns Promise
   *          A promise that resolves when everything has been refreshed.
   */
  refresh: function() {
    return Task.spawn(function*() {
      for (let [path, store] of this.localStores) {
        yield store.refresh();
      }
      emit(this, "refresh-complete");
    }.bind(this));
  },


  /**
   * Fetch a resource from the backing storage system for the store.
   *
   * @param string path
   *               The path to fetch
   * @param Object options
   *               "create": bool indicating whether to create a file if it does not exist.
   * @returns Promise
   *          A promise that resolves with the Resource.
   */
  resourceFor: function(path, options) {
    let store = this.storeContaining(path);
    return store.resourceFor(path, options);
  },

  /**
   * Get every resource used inside of the project.
   *
   * @returns Array<Resource>
   *          A list of all Resources in all Stores.
   */
  allResources: function() {
    let resources = [];
    for (let store of this.allStores()) {
      resources = resources.concat(store.allResources());
    }
    return resources;
  },

  /**
   * Get every Path used inside of the project.
   *
   * @returns generator-iterator<Store>
   *          A list of all Stores
   */
  allStores: function*() {
    for (let [path, store] of this.localStores) {
      yield store;
    }
  },

  /**
   * Get every file path used inside of the project.
   *
   * @returns Array<string>
   *          A list of all file paths
   */
  allPaths: function() {
    return [...this.localStores.keys()];
  },

  /**
   * Get the store that contains a path.
   *
   * @returns Store
   *          The store, if any.  Will return null if no store
   *          contains the given path.
   */
  storeContaining: function(path) {
    let containingStore = null;
    for (let store of this.allStores()) {
      if (store.contains(path)) {
        // With nested projects, the final containing store will be returned.
        containingStore = store;
      }
    }
    return containingStore;
  },

  /**
   * Add a store at the current path.  If a store already exists
   * for this path, then return it.
   *
   * @param string path
   * @returns LocalStore
   */
  addPath: function(path) {
    if (!this.localStores.has(path)) {
      this.addLocalStore(new LocalStore(path));
    }
    return this.localStores.get(path);
  },

  /**
   * Remove a store for a given path.
   *
   * @param string path
   */
  removePath: function(path) {
    this.removeLocalStore(this.localStores.get(path));
  },


  /**
   * Add the given Store to the project.
   * Fires a 'store-added' event on the project.
   *
   * @param Store store
   */
  addLocalStore: function(store) {
    store.canPair = true;
    this.localStores.set(store.path, store);

    // Originally StoreCollection.addStore
    on(this, store, "resource-added", (resource) => {
      emit(this, "resource-added", resource);
    });
    on(this, store, "resource-removed", (resource) => {
      emit(this, "resource-removed", resource);
    })

    emit(this, "store-added", store);
  },


  /**
   * Remove all of the Stores belonging to the project.
   */
  removeAllStores: function() {
    for (let store of this.allStores()) {
      this.removeLocalStore(store);
    }
  },

  /**
   * Remove the given Store from the project.
   * Fires a 'store-removed' event on the project.
   *
   * @param Store store
   */
  removeLocalStore: function(store) {
    // XXX: tree selection should be reset if active element is affected by
    // the store being removed
    if (store) {
      this.localStores.delete(store.path);
      forget(this, store);
      emit(this, "store-removed", store);
      store.destroy();
    }
  }
});

exports.Project = Project;
