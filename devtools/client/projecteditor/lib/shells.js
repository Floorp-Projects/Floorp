/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { EventTarget } = require("sdk/event/target");
const { emit } = require("sdk/event/core");
const { EditorTypeForResource } = require("devtools/client/projecteditor/lib/editors");
const NetworkHelper = require("devtools/shared/webconsole/network-helper");
const promise = require("promise");

/**
 * The Shell is the object that manages the editor for a single resource.
 * It is in charge of selecting the proper Editor (text/image/plugin-defined)
 * and instantiating / appending the editor.
 * This object is not exported, it is just used internally by the ShellDeck.
 *
 * This object has a promise `editorAppended`, that will resolve once the editor
 * is ready to be used.
 */
var Shell = Class({
  extends: EventTarget,

  /**
   * @param ProjectEditor host
   * @param Resource resource
   */
  initialize: function (host, resource) {
    this.host = host;
    this.doc = host.document;
    this.resource = resource;
    this.elt = this.doc.createElement("vbox");
    this.elt.classList.add("view-project-detail");
    this.elt.shell = this;

    let constructor = this._editorTypeForResource();

    this.editor = constructor(this.host);
    this.editor.shell = this;
    this.editorAppended = this.editor.appended;

    this.editor.on("load", () => {
      this.editorDeferred.resolve();
    });
    this.elt.appendChild(this.editor.elt);
  },

  /**
   * Start loading the resource.  The 'load' event happens as
   * a result of this function, so any listeners to 'editorAppended'
   * need to be added before calling this.
   */
  load: function () {
    this.editorDeferred = promise.defer();
    this.editorLoaded = this.editorDeferred.promise;
    this.editor.load(this.resource);
  },

  /**
   * Destroy the shell and its associated editor
   */
  destroy: function () {
    this.editor.destroy();
    this.resource.destroy();
  },

  /**
   * Make sure the correct editor is selected for the resource.
   * @returns Type:Editor
   */
  _editorTypeForResource: function () {
    let resource = this.resource;
    let constructor = EditorTypeForResource(resource);

    if (this.host.plugins) {
      this.host.plugins.forEach(plugin => {
        if (plugin.editorForResource) {
          let pluginEditor = plugin.editorForResource(resource);
          if (pluginEditor) {
            constructor = pluginEditor;
          }
        }
      });
    }

    return constructor;
  }
});

/**
 * The ShellDeck is in charge of managing the list of active Shells for
 * the current ProjectEditor instance (aka host).
 *
 * This object emits the following events:
 *   - "editor-created": When an editor is initially created
 *   - "editor-activated": When an editor is ready to use
 *   - "editor-deactivated": When an editor is ready to use
 */
var ShellDeck = Class({
  extends: EventTarget,

  /**
   * @param ProjectEditor host
   * @param Document document
   */
  initialize: function (host, document) {
    this.doc = document;
    this.host = host;
    this.deck = this.doc.createElement("deck");
    this.deck.setAttribute("flex", "1");
    this.elt = this.deck;

    this.shells = new Map();

    this._activeShell = null;
  },

  /**
   * Open a resource in a Shell.  Will create the Shell
   * if it doesn't exist yet.
   *
   * @param Resource resource
   *                 The file to be opened
   * @returns Shell
   */
  open: function (defaultResource) {
    let shell = this.shellFor(defaultResource);
    if (!shell) {
      shell = this._createShell(defaultResource);
      this.shells.set(defaultResource, shell);
    }
    this.selectShell(shell);
    return shell;
  },

  /**
   * Create a new Shell for a resource.  Called by `open`.
   *
   * @returns Shell
   */
  _createShell: function (defaultResource) {
    let shell = Shell(this.host, defaultResource);

    shell.editorAppended.then(() => {
      this.shells.set(shell.resource, shell);
      emit(this, "editor-created", shell.editor);
      if (this.currentShell === shell) {
        this.selectShell(shell);
      }

    });

    shell.load();
    this.deck.appendChild(shell.elt);
    return shell;
  },

  /**
   * Remove the shell for a given resource.
   *
   * @param Resource resource
   */
  removeResource: function (resource) {
    let shell = this.shellFor(resource);
    if (shell) {
      this.shells.delete(resource);
      shell.destroy();
    }
  },

  destroy: function () {
    for (let [resource, shell] of this.shells.entries()) {
      this.shells.delete(resource);
      shell.destroy();
    }
  },

  /**
   * Select a given shell and open its editor.
   * Will fire editor-deactivated on the old selected Shell (if any),
   * and editor-activated on the new one once it is ready
   *
   * @param Shell shell
   */
  selectShell: function (shell) {
    // Don't fire another activate if this is already the active shell
    if (this._activeShell != shell) {
      if (this._activeShell) {
        emit(this, "editor-deactivated", this._activeShell.editor, this._activeShell.resource);
      }
      this.deck.selectedPanel = shell.elt;
      this._activeShell = shell;

      // Only reload the shell if the editor doesn't have local changes.
      if (shell.editor.isClean()) {
        shell.load();
      }
      shell.editorLoaded.then(() => {
        // Handle case where another shell has been requested before this
        // one is finished loading.
        if (this._activeShell === shell) {
          emit(this, "editor-activated", shell.editor, shell.resource);
        }
      });
    }
  },

  /**
   * Find a Shell for a Resource.
   *
   * @param Resource resource
   * @returns Shell
   */
  shellFor: function (resource) {
    return this.shells.get(resource);
  },

  /**
   * The currently active Shell.  Note: the editor may not yet be available
   * on the current shell.  Best to wait for the 'editor-activated' event
   * instead.
   *
   * @returns Shell
   */
  get currentShell() {
    return this._activeShell;
  },

  /**
   * The currently active Editor, or null if it is not ready.
   *
   * @returns Editor
   */
  get currentEditor() {
    let shell = this.currentShell;
    return shell ? shell.editor : null;
  },

});
exports.ShellDeck = ShellDeck;
