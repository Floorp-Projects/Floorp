/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const { TextEncoder, TextDecoder } = require('sdk/io/buffer');
const { Class } = require("sdk/core/heritage");
const { EventTarget } = require("sdk/event/target");
const { emit } = require("sdk/event/core");
const URL = require("sdk/url");
const promise = require("promise");
const { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});
const { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});

const gDecoder = new TextDecoder();
const gEncoder = new TextEncoder();

/**
 * A Resource is a single file-like object that can be respresented
 * as a file for ProjectEditor.
 *
 * The Resource class is not exported, and should not be instantiated
 * Instead, you should use the FileResource class that extends it.
 *
 * This object emits the following events:
 *   - "children-changed": When a child has been added or removed.
 *                         See setChildren.
 *   - "deleted": When the resource has been deleted.
 */
var Resource = Class({
  extends: EventTarget,

  refresh: function() { return promise.resolve(this); },
  destroy: function() { },
  delete: function() { },

  setURI: function(uri) {
    if (typeof(uri) === "string") {
      uri = URL.URL(uri);
    }
    this.uri = uri;
  },

  /**
   * Is there more than 1 child Resource?
   */
  get hasChildren() { return this.children && this.children.size > 0; },

  /**
   * Is this Resource the root (top level for the store)?
   */
  get isRoot() {
    return !this.parent
  },

  /**
   * Sorted array of children for display
   */
  get childrenSorted() {
    if (!this.hasChildren) {
      return [];
    }

    return [...this.children].sort((a, b)=> {
      // Put directories above files.
      if (a.isDir !== b.isDir) {
        return b.isDir;
      }
      return a.basename.toLowerCase() > b.basename.toLowerCase();
    });
  },

  /**
   * Set the children set of this Resource, and notify of any
   * additions / removals that happened in the change.
   */
  setChildren: function(newChildren) {
    let oldChildren = this.children || new Set();
    let change = false;

    for (let child of oldChildren) {
      if (!newChildren.has(child)) {
        change = true;
        child.parent = null;
        this.store.notifyRemove(child);
      }
    }

    for (let child of newChildren) {
      if (!oldChildren.has(child)) {
        change = true;
        child.parent = this;
        this.store.notifyAdd(child);
      }
    }

    this.children = newChildren;
    if (change) {
      emit(this, "children-changed", this);
    }
  },

  /**
   * Add a resource to children set and notify of the change.
   *
   * @param Resource resource
   */
  addChild: function(resource) {
    this.children = this.children || new Set();

    resource.parent = this;
    this.children.add(resource);
    this.store.notifyAdd(resource);
    emit(this, "children-changed", this);
    return resource;
  },

  /**
   * Checks if current object has child with specific name.
   *
   * @param string name
   */
  hasChild: function(name) {
    for (let child of this.children) {
      if (child.basename === name) {
        return true;
      }
    }
    return false;
  },

  /**
   * Remove a resource to children set and notify of the change.
   *
   * @param Resource resource
   */
  removeChild: function(resource) {
    resource.parent = null;
    this.children.remove(resource);
    this.store.notifyRemove(resource);
    emit(this, "children-changed", this);
    return resource;
  },

  /**
   * Return a set with children, children of children, etc -
   * gathered recursively.
   *
   * @returns Set<Resource>
   */
  allDescendants: function() {
    let set = new Set();

    function addChildren(item) {
      if (!item.children) {
        return;
      }

      for (let child of item.children) {
        set.add(child);
      }
    }

    addChildren(this);
    for (let item of set) {
      addChildren(item);
    }

    return set;
  },
});

/**
 * A FileResource is an implementation of Resource for a File System
 * backing.  This is exported, and should be used instead of Resource.
 */
var FileResource = Class({
  extends: Resource,

  /**
   * @param Store store
   * @param String path
   * @param FileInfo info
   *        https://developer.mozilla.org/en-US/docs/JavaScript_OS.File/OS.File.Info
   */
  initialize: function(store, path, info) {
    this.store = store;
    this.path = path;

    this.setURI(URL.URL(URL.fromFilename(path)));
    this._lastReadModification = undefined;

    this.info = info;
    this.parent = null;
  },

  toString: function() {
    return "[FileResource:" + this.path + "]";
  },

  destroy: function() {
    if (this._refreshDeferred) {
      this._refreshDeferred.reject();
    }
    this._refreshDeferred = null;
  },

  /**
   * Fetch and cache information about this particular file.
   * https://developer.mozilla.org/en-US/docs/JavaScript_OS.File/OS.File_for_the_main_thread#OS.File.stat
   *
   * @returns Promise
   *          Resolves once the File.stat has finished.
   */
  refresh: function() {
    if (this._refreshDeferred) {
      return this._refreshDeferred.promise;
    }
    this._refreshDeferred = promise.defer();
    OS.File.stat(this.path).then(info => {
      this.info = info;
      if (this._refreshDeferred) {
        this._refreshDeferred.resolve(this);
        this._refreshDeferred = null;
      }
    });
    return this._refreshDeferred.promise;
  },

  /**
   * Return the trailing name component of this Resource
   */
  get basename() {
    return this.path.replace(/\/+$/, '').replace(/\\/g,'/').replace( /.*\//, '' );
  },

  /**
   * A string to be used when displaying this Resource in views
   */
  get displayName() {
    return this.basename + (this.isDir ? "/" : "")
  },

  /**
   * Is this FileResource a directory?  Rather than checking children
   * here, we use this.info.  So this could return a false negative
   * if there was no info passed in on constructor and the first
   * refresh hasn't yet finished.
   */
  get isDir() {
    if (!this.info) { return false; }
    return this.info.isDir && !this.info.isSymLink;
  },

  /**
   * Read the file as a string asynchronously.
   *
   * @returns Promise
   *          Resolves with the text of the file.
   */
  load: function() {
    return OS.File.read(this.path).then(bytes => {
      return gDecoder.decode(bytes);
    });
  },

  /**
   * Delete the file from the filesystem
   *
   * @returns Promise
   *          Resolves when the file is deleted
   */
  delete: function() {
    emit(this, "deleted", this);
    if (this.isDir) {
      return OS.File.removeDir(this.path);
    } else {
      return OS.File.remove(this.path);
    }
  },

  /**
   * Add a text file as a child of this FileResource.
   * This instance must be a directory.
   *
   * @param string name
   *               The filename (path will be generated based on this.path).
   *        string initial
   *               The content to write to the new file.
   * @returns Promise
   *          Resolves with the new FileResource once it has
   *          been written to disk.
   *          Rejected if this is not a directory.
   */
  createChild: function(name, initial="") {
    if (!this.isDir) {
      return promise.reject(new Error("Cannot add child to a regular file"));
    }

    let newPath = OS.Path.join(this.path, name);

    let buffer = initial ? gEncoder.encode(initial) : "";
    return OS.File.writeAtomic(newPath, buffer, {
      noOverwrite: true
    }).then(() => {
      return this.store.refresh();
    }).then(() => {
      let resource = this.store.resources.get(newPath);
      if (!resource) {
        throw new Error("Error creating " + newPath);
      }
      return resource;
    });
  },

  /**
   * Rename the file from the filesystem
   *
   * @returns Promise
   *          Resolves with the renamed FileResource.
   */
  rename: function(oldName, newName) {
    let oldPath = OS.Path.join(this.path, oldName);
    let newPath = OS.Path.join(this.path, newName);

    return OS.File.move(oldPath, newPath).then(() => {
      return this.store.refresh();
    }).then(() => {
      let resource = this.store.resources.get(newPath);
      if (!resource) {
        throw new Error("Error creating " + newPath);
      }
      return resource;
    });
  },

  /**
   * Write a string to this file.
   *
   * @param string content
   * @returns Promise
   *          Resolves once it has been written to disk.
   *          Rejected if there is an error
   */
  save: Task.async(function*(content) {
    // XXX: writeAtomic was losing permissions after saving on OSX
    // return OS.File.writeAtomic(this.path, buffer, { tmpPath: this.path + ".tmp" });
    let buffer = gEncoder.encode(content);
    let path = this.path;
    let file = yield OS.File.open(path, {truncate: true});
    yield file.write(buffer);
    yield file.close();
  }),

  /**
   * Attempts to get the content type from the file.
   */
  get contentType() {
    if (this._contentType) {
      return this._contentType;
    }
    if (this.isDir) {
      return "x-directory/normal";
    }
    try {
      this._contentType = mimeService.getTypeFromFile(new FileUtils.File(this.path));
    } catch(ex) {
      if (ex.name !== "NS_ERROR_NOT_AVAILABLE" &&
          ex.name !== "NS_ERROR_FAILURE") {
        console.error(ex, this.path);
      }
      this._contentType = null;
    }
    return this._contentType;
  },

  /**
   * A string used when determining the type of Editor to open for this.
   * See editors.js -> EditorTypeForResource.
   */
  get contentCategory() {
    const NetworkHelper = require("devtools/shared/webconsole/network-helper");
    let category = NetworkHelper.mimeCategoryMap[this.contentType];
    // Special treatment for manifest.webapp.
    if (!category && this.basename === "manifest.webapp") {
      return "json";
    }
    return category || "txt";
  }
});

exports.FileResource = FileResource;
