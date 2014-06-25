/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cc, Ci, Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { Project } = require("projecteditor/project");
const { ProjectTreeView } = require("projecteditor/tree");
const { ShellDeck } = require("projecteditor/shells");
const { Resource } = require("projecteditor/stores/resource");
const { registeredPlugins } = require("projecteditor/plugins/core");
const { EventTarget } = require("sdk/event/target");
const { on, forget } = require("projecteditor/helpers/event");
const { emit } = require("sdk/event/core");
const { merge } = require("sdk/util/object");
const promise = require("projecteditor/helpers/promise");
const { ViewHelpers } = Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});
const { DOMHelpers } = Cu.import("resource:///modules/devtools/DOMHelpers.jsm");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const ITCHPAD_URL = "chrome://browser/content/devtools/projecteditor.xul";

// Enabled Plugins
require("projecteditor/plugins/dirty/dirty");
require("projecteditor/plugins/delete/delete");
require("projecteditor/plugins/new/new");
require("projecteditor/plugins/save/save");
require("projecteditor/plugins/image-view/plugin");
require("projecteditor/plugins/app-manager/plugin");
require("projecteditor/plugins/status-bar/plugin");

// Uncomment to enable logging.
// require("projecteditor/plugins/logging/logging");

/**
 * This is the main class tying together an instance of the ProjectEditor.
 * The frontend is contained inside of this.iframe, which loads projecteditor.xul.
 *
 * Usage:
 *   let projecteditor = new ProjectEditor(frame);
 *   projecteditor.loaded.then((projecteditor) => {
 *      // Ready to use.
 *   });
 *
 * Responsible for maintaining:
 *   - The list of Plugins for this instance.
 *   - The ShellDeck, which includes all Shells for opened Resources
 *   -- Shells take in a Resource, and construct the appropriate Editor
 *   - The Project, which includes all Stores for this instance
 *   -- Stores manage all Resources starting from a root directory
 *   --- Resources are a representation of a file on disk
 *   - The ProjectTreeView that builds the UI for interacting with the
 *     project.
 *
 * This object emits the following events:
 *   - "onEditorDestroyed": When editor is destroyed
 *   - "onEditorSave": When editor is saved
 *   - "onEditorLoad": When editor is loaded
 *   - "onEditorActivated": When editor is activated
 *   - "onEditorChange": When editor is changed
 *   - "onEditorCursorActivity": When there is cursor activity in a text editor
 *   - "onCommand": When a command happens
 *   - "onEditorDestroyed": When editor is destroyed
 *
 * The events can be bound like so:
 *   projecteditor.on("onEditorCreated", (editor) => { });
 */
var ProjectEditor = Class({
  extends: EventTarget,

  /**
   * Initialize ProjectEditor, and load into an iframe if specified.
   *
   * @param Iframe iframe
   *        The iframe to inject the DOM into.  If this is not
   *        specified, then this.load(frame) will need to be called
   *        before accessing ProjectEditor.
   */
  initialize: function(iframe) {
    this._onTreeSelected = this._onTreeSelected.bind(this);
    this._onTreeResourceRemoved = this._onTreeResourceRemoved.bind(this);
    this._onEditorCreated = this._onEditorCreated.bind(this);
    this._onEditorActivated = this._onEditorActivated.bind(this);
    this._onEditorDeactivated = this._onEditorDeactivated.bind(this);
    this._updateEditorMenuItems = this._updateEditorMenuItems.bind(this);

    if (iframe) {
      this.load(iframe);
    }
  },

  /**
   * Load the instance inside of a specified iframe.
   * This can be called more than once, and it will return the promise
   * from the first call.
   *
   * @param Iframe iframe
   *        The iframe to inject the projecteditor DOM into
   * @returns Promise
   *          A promise that is resolved once the iframe has been
   *          loaded.
   */
  load: function(iframe) {
    if (this.loaded) {
      return this.loaded;
    }

    let deferred = promise.defer();
    this.loaded = deferred.promise;
    this.iframe = iframe;

    let domReady = () => {
      this._onLoad();
      deferred.resolve(this);
    };

    let domHelper = new DOMHelpers(this.iframe.contentWindow);
    domHelper.onceDOMReady(domReady);

    this.iframe.setAttribute("src", ITCHPAD_URL);

    return this.loaded;
  },

  /**
   * Build the projecteditor DOM inside of this.iframe.
   */
  _onLoad: function() {
    this.document = this.iframe.contentDocument;
    this.window = this.iframe.contentWindow;

    this._buildSidebar();

    this.window.addEventListener("unload", this.destroy.bind(this));

    // Editor management
    this.shells = new ShellDeck(this, this.document);
    this.shells.on("editor-created", this._onEditorCreated);
    this.shells.on("editor-activated", this._onEditorActivated);
    this.shells.on("editor-deactivated", this._onEditorDeactivated);

    let shellContainer = this.document.querySelector("#shells-deck-container");
    shellContainer.appendChild(this.shells.elt);

    let popup = this.document.querySelector("#edit-menu-popup");
    popup.addEventListener("popupshowing", this.updateEditorMenuItems);

    // We are not allowing preset projects for now - rebuild a fresh one
    // each time.
    this.setProject(new Project({
      id: "",
      name: "",
      directories: [],
      openFiles: []
    }));

    this._initCommands();
    this._initPlugins();
  },


  /**
   * Create the project tree sidebar that lists files.
   */
  _buildSidebar: function() {
    this.projectTree = new ProjectTreeView(this.document, {
      resourceVisible: this.resourceVisible.bind(this),
      resourceFormatter: this.resourceFormatter.bind(this)
    });
    on(this, this.projectTree, "selection", this._onTreeSelected);
    on(this, this.projectTree, "resource-removed", this._onTreeResourceRemoved);

    let sourcesBox = this.document.querySelector("#sources > vbox");
    sourcesBox.appendChild(this.projectTree.elt);
  },

  /**
   * Set up listeners for commands to dispatch to all of the plugins
   */
  _initCommands: function() {
    this.commands = this.document.querySelector("#projecteditor-commandset");
    this.commands.addEventListener("command", (evt) => {
      evt.stopPropagation();
      evt.preventDefault();
      this.pluginDispatch("onCommand", evt.target.id, evt.target);
    });
  },

  /**
   * Initialize each plugin in registeredPlugins
   */
  _initPlugins: function() {
    this._plugins = [];

    for (let plugin of registeredPlugins) {
      try {
        this._plugins.push(plugin(this));
      } catch(ex) {
        console.exception(ex);
      }
    }

    this.pluginDispatch("lateInit");
  },

  /**
   * Enable / disable necessary menu items using globalOverlay.js.
   */
  _updateEditorMenuItems: function() {
    this.window.goUpdateGlobalEditMenuItems();
    this.window.goUpdateGlobalEditMenuItems();
    let commands = ['cmd_undo', 'cmd_redo', 'cmd_delete', 'cmd_findAgain'];
    commands.forEach(this.window.goUpdateCommand);
  },

  /**
   * Destroy all objects on the iframe unload event.
   */
  destroy: function() {
    this._plugins.forEach(plugin => { plugin.destroy(); });

    forget(this, this.projectTree);
    this.projectTree.destroy();
    this.projectTree = null;

    this.shells.destroy();

    forget(this, this.project);
    this.project.destroy();
    this.project = null;
  },

  /**
   * Set the current project viewed by the projecteditor.
   *
   * @param Project project
   *        The project to set.
   */
  setProject: function(project) {
    if (this.project) {
      forget(this, this.project);
    }
    this.project = project;
    this.projectTree.setProject(project);

    // Whenever a store gets removed, clean up any editors that
    // exist for resources within it.
    on(this, project, "store-removed", (store) => {
      store.allResources().forEach((resource) => {
        this.shells.removeResource(resource);
      });
    });
  },

  /**
   * Set the current project viewed by the projecteditor to a single path,
   * used by the app manager.
   *
   * @param string path
   *               The file path to set
   * @param Object opts
   *               Custom options used by the project.
   *                - name: display name for project
   *                - iconUrl: path to icon for project
   *                - validationStatus: one of 'unknown|error|warning|valid'
   *                - projectOverviewURL: path to load for iframe when project
   *                    is selected in the tree.
   * @param Promise
   *        Promise that is resolved once the project is ready to be used.
   */
  setProjectToAppPath: function(path, opts = {}) {
    this.project.appManagerOpts = opts;

    let existingPaths = this.project.allPaths();
    if (existingPaths.length !== 1 || existingPaths[0] !== path) {
      // Only fully reset if this is a new path.
      this.project.removeAllStores();
      this.project.addPath(path);
    } else {
      // Otherwise, just ask for the root to be redrawn
      let rootResource = this.project.localStores.get(path).root;
      emit(rootResource, "label-change", rootResource);
    }

    return this.project.refresh();
  },

  /**
   * Open a resource in a particular shell.
   *
   * @param Resource resource
   *                 The file to be opened.
   */
  openResource: function(resource) {
    this.shells.open(resource);
    this.projectTree.selectResource(resource);
  },

  /**
   * When a node is selected in the tree, open its associated editor.
   *
   * @param Resource resource
   *                 The file that has been selected
   */
  _onTreeSelected: function(resource) {
    // Don't attempt to open a directory that is not the root element.
    if (resource.isDir && resource.parent) {
      return;
    }
    this.pluginDispatch("onTreeSelected", resource);
    this.openResource(resource);
  },

  /**
   * When a node is removed, destroy it and its associated editor.
   *
   * @param Resource resource
   *                 The resource being removed
   */
  _onTreeResourceRemoved: function(resource) {
    this.shells.removeResource(resource);
  },

  /**
   * Create an xul element with options
   *
   * @param string type
   *               The tag name of the element to create.
   * @param Object options
   *               "command": DOMNode or string ID of a command element.
   *               "parent": DOMNode or selector of parent to append child to.
   *               anything other keys are set as an attribute as the element.
   * @returns DOMElement
   *          The element that has been created.
   */
  createElement: function(type, options) {
    let elt = this.document.createElement(type);

    let parent;

    for (let opt in options) {
      if (opt === "command") {
        let command = typeof(options.command) === "string" ? options.command : options.command.id;
        elt.setAttribute("command", command);
      } else if (opt === "parent") {
        continue;
      } else {
        elt.setAttribute(opt, options[opt]);
      }
    }

    if (options.parent) {
      let parent = options.parent;
      if (typeof(parent) === "string") {
        parent = this.document.querySelector(parent);
      }
      parent.appendChild(elt);
    }

    return elt;
  },

  /**
   * Create a "menuitem" xul element with options
   *
   * @param Object options
   *               See createElement for available options.
   * @returns DOMElement
   *          The menuitem that has been created.
   */
  createMenuItem: function(options) {
    return this.createElement("menuitem", options);
  },

  /**
   * Add a command to the projecteditor document.
   * This method is meant to be used with plugins.
   *
   * @param Object definition
   *               key: a key/keycode string. Example: "f".
   *               id: Unique ID.  Example: "find".
   *               modifiers: Key modifiers. Example: "accel".
   * @returns DOMElement
   *          The command element that has been created.
   */
  addCommand: function(definition) {
    let command = this.document.createElement("command");
    command.setAttribute("id", definition.id);
    if (definition.key) {
      let key = this.document.createElement("key");
      key.id = "key_" + definition.id;

      let keyName = definition.key;
      if (keyName.startsWith("VK_")) {
        key.setAttribute("keycode", keyName);
      } else {
        key.setAttribute("key", keyName);
      }
      key.setAttribute("modifiers", definition.modifiers);
      key.setAttribute("command", definition.id);
      this.document.getElementById("projecteditor-keyset").appendChild(key);
    }
    command.setAttribute("oncommand", "void(0);"); // needed. See bug 371900
    this.document.getElementById("projecteditor-commandset").appendChild(command);
    return command;
  },

  /**
   * Get the instance of a plugin registered with a certain type.
   *
   * @param Type pluginType
   *             The type, such as SavePlugin
   * @returns Plugin
   *          The plugin instance matching the specified type.
   */
  getPlugin: function(pluginType) {
    for (let plugin of this.plugins) {
      if (plugin.constructor === pluginType) {
        return plugin;
      }
    }
    return null;
  },

  /**
   * Get all plugin instances active for the current project
   *
   * @returns [Plugin]
   */
  get plugins() {
    if (!this._plugins) {
      console.log("plugins requested before _plugins was set");
      return [];
    }
    // Could filter further based on the type of project selected,
    // but no need right now.
    return this._plugins;
  },

  /**
   * Dispatch an onEditorCreated event, and listen for other events specific
   * to this editor instance.
   *
   * @param Editor editor
   *               The new editor instance.
   */
  _onEditorCreated: function(editor) {
    this.pluginDispatch("onEditorCreated", editor);
    this._editorListenAndDispatch(editor, "change", "onEditorChange");
    this._editorListenAndDispatch(editor, "cursorActivity", "onEditorCursorActivity");
    this._editorListenAndDispatch(editor, "load", "onEditorLoad");
    this._editorListenAndDispatch(editor, "save", "onEditorSave");
  },

  /**
   * Dispatch an onEditorActivated event and finish setting up once the
   * editor is ready to use.
   *
   * @param Editor editor
   *               The editor instance, which is now appended in the document.
   * @param Resource resource
   *               The resource used by the editor
   */
  _onEditorActivated: function(editor, resource) {
    editor.setToolbarVisibility();
    this.pluginDispatch("onEditorActivated", editor, resource);
  },

  /**
   * Dispatch an onEditorDactivated event once an editor loses focus
   *
   * @param Editor editor
   *               The editor instance, which is no longer active.
   * @param Resource resource
   *               The resource used by the editor
   */
  _onEditorDeactivated: function(editor, resource) {
    this.pluginDispatch("onEditorDeactivated", editor, resource);
  },

  /**
   * Call a method on all plugins that implement the method.
   * Also emits the same handler name on `this`.
   *
   * @param string handler
   *               Which function name to call on plugins.
   * @param ...args args
   *                All remaining parameters are passed into the handler.
   */
  pluginDispatch: function(handler, ...args) {
    // XXX: Memory leak when console.log an Editor here
    // console.log("DISPATCHING EVENT TO PLUGIN", handler, args);
    emit(this, handler, ...args);
    this.plugins.forEach(plugin => {
      try {
        if (handler in plugin) plugin[handler](...args);
      } catch(ex) {
        console.error(ex);
      }
    })
  },

  /**
   * Listen to an event on the editor object and dispatch it
   * to all plugins that implement the associated method
   *
   * @param Editor editor
   *               Which editor to listen to
   * @param string event
   *               Which editor event to listen for
   * @param string handler
   *               Which plugin method to call
   */
  _editorListenAndDispatch: function(editor, event, handler) {
    /// XXX: Uncommenting this line also causes memory leak.
    // console.log("Binding listen and dispatch", editor);
    editor.on(event, (...args) => {
      this.pluginDispatch(handler, editor, this.resourceFor(editor), ...args);
    });
  },

  /**
   * Find a shell for a resource.
   *
   * @param Resource resource
   *                 The file to be opened.
   * @returns Shell
   */
  shellFor: function(resource) {
    return this.shells.shellFor(resource);
  },

  /**
   * Returns the Editor for a given resource.
   *
   * @param Resource resource
   *                 The file to check.
   * @returns Editor
   *          Instance of the editor for this file.
   */
  editorFor: function(resource) {
    let shell = this.shellFor(resource);
    return shell ? shell.editor : shell;
  },

  /**
   * Returns a resource for the given editor
   *
   * @param Editor editor
   *               The editor to check
   * @returns Resource
   *          The resource associated with this editor
   */
  resourceFor: function(editor) {
    if (editor && editor.shell && editor.shell.resource) {
      return editor.shell.resource;
    }
    return null;
  },

  /**
   * Decide whether a given resource should be hidden in the tree.
   *
   * @param Resource resource
   *                 The resource in the tree
   * @returns Boolean
   *          True if the node should be visible, false if hidden.
   */
  resourceVisible: function(resource) {
    return true;
  },

  /**
   * Format the given node for display in the resource tree view.
   *
   * @param Resource resource
   *                 The file to be opened.
   * @param DOMNode elt
   *                The element in the tree to render into.
   */
  resourceFormatter: function(resource, elt) {
    let editor = this.editorFor(resource);
    let renderedByPlugin = false;

    // Allow plugins to override default templating of resource in tree.
    this.plugins.forEach(plugin => {
      if (!plugin.onAnnotate) {
        return;
      }
      if (plugin.onAnnotate(resource, editor, elt)) {
        renderedByPlugin = true;
      }
    });

    // If no plugin wants to handle it, just use a string from the resource.
    if (!renderedByPlugin) {
      elt.textContent = resource.displayName;
    }
  },

  get sourcesVisible() {
    return this.sourceToggle.hasAttribute("pane-collapsed");
  },

  get currentShell() {
    return this.shells.currentShell;
  },

  get currentEditor() {
    return this.shells.currentEditor;
  },
});

exports.ProjectEditor = ProjectEditor;
