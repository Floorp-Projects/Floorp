/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Cu.import("resource://gre/modules/osfile.jsm");
const {VariablesView} =
  Cu.import("resource:///modules/devtools/VariablesView.jsm", {});

const VARIABLES_VIEW_URL =
  "chrome://browser/content/devtools/widgets/VariablesView.xul";

function ManifestEditor(project) {
  this.project = project;
  this._onContainerReady = this._onContainerReady.bind(this);
  this._onEval = this._onEval.bind(this);
  this._onSwitch = this._onSwitch.bind(this);
  this._onDelete = this._onDelete.bind(this);
  this._onNew = this._onNew.bind(this);
}

ManifestEditor.prototype = {
  get manifest() { return this.project.manifest; },

  get editable() { return this.project.type == "packaged"; },

  show: function(containerElement) {
    let deferred = promise.defer();
    let iframe = this._iframe = document.createElement("iframe");

    iframe.addEventListener("load", function onIframeLoad() {
      iframe.removeEventListener("load", onIframeLoad, true);
      deferred.resolve(iframe.contentWindow);
    }, true);

    iframe.setAttribute("src", VARIABLES_VIEW_URL);
    iframe.classList.add("variables-view");
    containerElement.appendChild(iframe);

    return deferred.promise.then(this._onContainerReady);
  },

  _onContainerReady: function(varWindow) {
    let variablesContainer = varWindow.document.querySelector("#variables");

    variablesContainer.classList.add("manifest-editor");

    let editor = this.editor = new VariablesView(variablesContainer);

    editor.onlyEnumVisible = true;
    editor.alignedValues = true;
    editor.actionsFirst = true;

    if (this.editable) {
      editor.eval = this._onEval;
      editor.switch = this._onSwitch;
      editor.delete = this._onDelete;
      editor.new = this._onNew;
    }

    return this.update();
  },

  _onEval: function(variable, value) {
    let parent = this._descend(variable.ownerView.symbolicPath);
    try {
      parent[variable.name] = JSON.parse(value);
    } catch(e) {
      Cu.reportError(e);
    }

    this.update();
  },

  _onSwitch: function(variable, newName) {
    if (variable.name == newName) {
      return;
    }

    let parent = this._descend(variable.ownerView.symbolicPath);
    parent[newName] = parent[variable.name];
    delete parent[variable.name];

    this.update();
  },

  _onDelete: function(variable) {
    let parent = this._descend(variable.ownerView.symbolicPath);
    delete parent[variable.name];
  },

  _onNew: function(variable, newName, newValue) {
    let parent = this._descend(variable.symbolicPath);
    try {
      parent[newName] = JSON.parse(newValue);
    } catch(e) {
      Cu.reportError(e);
    }

    this.update();
  },

  /**
   * Returns the value located at a given path in the manifest.
   * @param path array
   *        A string for each path component: ["developer", "name"]
   */
  _descend: function(path) {
    let parent = this.manifest;
    while (path.length) {
      parent = parent[path.shift()];
    }
    return parent;
  },

  update: function() {
    this.editor.rawObject = this.manifest;
    this.editor.commitHierarchy();

    // Wait until the animation from commitHierarchy has completed
    let deferred = promise.defer();
    setTimeout(deferred.resolve, this.editor.lazyEmptyDelay + 1);
    return deferred.promise;
  },

  save: function() {
    if (this.editable) {
      let validator = new AppValidator(this.project);
      let manifestFile = validator._getPackagedManifestFile();
      let path = manifestFile.path;

      let encoder = new TextEncoder();
      let data = encoder.encode(JSON.stringify(this.manifest, null, 2));

      return OS.File.writeAtomic(path, data, { tmpPath: path + ".tmp" });
    }

    return promise.resolve();
  },

  destroy: function() {
    if (this._iframe) {
      this._iframe.remove();
    }
  }
};
