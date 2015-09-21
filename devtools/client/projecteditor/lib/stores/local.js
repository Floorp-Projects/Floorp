/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cc, Ci, Cu, ChromeWorker } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});
const { emit } = require("sdk/event/core");
const { Store } = require("projecteditor/stores/base");
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
const promise = require("promise");
const { on, forget } = require("projecteditor/helpers/event");
const { FileResource } = require("projecteditor/stores/resource");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {setTimeout, clearTimeout} = Cu.import("resource://gre/modules/Timer.jsm", {});

const CHECK_LINKED_DIRECTORY_DELAY = 5000;
const SHOULD_LIVE_REFRESH = true;
// XXX: Ignores should be customizable
const IGNORE_REGEX = /(^\.)|(\~$)|(^node_modules$)/;

/**
 * A LocalStore object maintains a collection of Resource objects
 * from the file system.
 *
 * This object emits the following events:
 *   - "resource-added": When a resource is added
 *   - "resource-removed": When a resource is removed
 */
var LocalStore = Class({
  extends: Store,

  defaultCategory: "js",

  initialize: function(path) {
    this.initStore();
    this.path = OS.Path.normalize(path);
    this.rootPath = this.path;
    this.displayName = this.path;
    this.root = this._forPath(this.path);
    this.notifyAdd(this.root);
    this.refreshLoop = this.refreshLoop.bind(this);
    this.refreshLoop();
  },

  destroy: function() {
    clearTimeout(this._refreshTimeout);

    if (this._refreshDeferred) {
      this._refreshDeferred.reject("destroy");
    }
    if (this.worker) {
      this.worker.terminate();
    }

    this._refreshTimeout = null;
    this._refreshDeferred = null;
    this.worker = null;

    if (this.root) {
      forget(this, this.root);
      this.root.destroy();
    }
  },

  toString: function() { return "[LocalStore:" + this.path + "]" },

  /**
   * Return a FileResource object for the given path.  If a FileInfo
   * is provided the resource will use it, otherwise the FileResource
   * might not have full information until the next refresh.
   *
   * The following parameters are passed into the FileResource constructor
   * See resource.js for information about them
   *
   * @param String path
   * @param FileInfo info
   * @returns Resource
   */
  _forPath: function(path, info=null) {
    if (this.resources.has(path)) {
      return this.resources.get(path);
    }

    let resource = FileResource(this, path, info);
    this.resources.set(path, resource);
    return resource;
  },

  /**
   * Return a promise that resolves to a fully-functional FileResource
   * within this project.  This will hit the disk for stat info.
   * options:
   *
   *   create: If true, a resource will be created even if the underlying
   *     file doesn't exist.
   */
  resourceFor: function(path, options) {
    path = OS.Path.normalize(path);

    if (this.resources.has(path)) {
      return promise.resolve(this.resources.get(path));
    }

    if (!this.contains(path)) {
      return promise.reject(new Error(path + " does not belong to " + this.path));
    }

    return Task.spawn(function*() {
      let parent = yield this.resourceFor(OS.Path.dirname(path));

      let info;
      try {
        info = yield OS.File.stat(path);
      } catch (ex if ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
        if (!options.create) {
          throw ex;
        }
      }

      let resource = this._forPath(path, info);
      parent.addChild(resource);
      return resource;
    }.bind(this));
  },

  refreshLoop: function() {
    // XXX: Once Bug 958280 adds a watch function, will not need to forever loop here.
    this.refresh().then(() => {
      if (SHOULD_LIVE_REFRESH) {
        this._refreshTimeout = setTimeout(this.refreshLoop,
          CHECK_LINKED_DIRECTORY_DELAY);
      }
    });
  },

  _refreshTimeout: null,
  _refreshDeferred: null,

  /**
   * Refresh the directory structure.
   */
  refresh: function(path=this.rootPath) {
    if (this._refreshDeferred) {
      return this._refreshDeferred.promise;
    }
    this._refreshDeferred = promise.defer();

    let worker = this.worker = new ChromeWorker("chrome://devtools/content/projecteditor/lib/helpers/readdir.js");
    let start = Date.now();

    worker.onmessage = evt => {
      // console.log("Directory read finished in " + ( Date.now() - start ) +"ms", evt);
      for (path in evt.data) {
        let info = evt.data[path];
        info.path = path;

        let resource = this._forPath(path, info);
        resource.info = info;
        if (info.isDir) {
          let newChildren = new Set();
          for (let childPath of info.children) {
            childInfo = evt.data[childPath];
            newChildren.add(this._forPath(childPath, childInfo));
          }
          resource.setChildren(newChildren);
        }
        resource.info.children = null;
      }

      worker = null;
      this._refreshDeferred.resolve();
      this._refreshDeferred = null;
    };
    worker.onerror = ex => {
      console.error(ex);
      worker = null;
      this._refreshDeferred.reject(ex);
      this._refreshDeferred = null;
    }
    worker.postMessage({ path: this.rootPath, ignore: IGNORE_REGEX });
    return this._refreshDeferred.promise;
  },

  /**
   * Returns true if the given path would be a child of the store's
   * root directory.
   */
  contains: function(path) {
    path = OS.Path.normalize(path);
    let thisPath = OS.Path.split(this.rootPath);
    let thatPath = OS.Path.split(path)

    if (!(thisPath.absolute && thatPath.absolute)) {
      throw new Error("Contains only works with absolute paths.");
    }

    if (thisPath.winDrive && (thisPath.winDrive != thatPath.winDrive)) {
      return false;
    }

    if (thatPath.components.length <= thisPath.components.length) {
      return false;
    }

    for (let i = 0; i < thisPath.components.length; i++) {
      if (thisPath.components[i] != thatPath.components[i]) {
        return false;
      }
    }
    return true;
  }
});
exports.LocalStore = LocalStore;

