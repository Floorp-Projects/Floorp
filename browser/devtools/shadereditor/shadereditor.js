/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");
Cu.import("resource:///modules/devtools/SideMenuWidget.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
const promise = require("sdk/core/promise");
const EventEmitter = require("devtools/shared/event-emitter");
const Editor = require("devtools/sourceeditor/editor");

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When the vertex and fragment sources were shown in the editor.
  SOURCES_SHOWN: "ShaderEditor:SourcesShown",
  // When a shader's source was edited and compiled via the editor.
  SHADER_COMPILED: "ShaderEditor:ShaderCompiled"
};

const STRINGS_URI = "chrome://browser/locale/devtools/shadereditor.properties"
const HIGHLIGHT_COLOR = [1, 0, 0, 1];
const BLACKBOX_COLOR = [0, 0, 0, 0];
const TYPING_MAX_DELAY = 500;
const SHADERS_AUTOGROW_ITEMS = 4;
const DEFAULT_EDITOR_CONFIG = {
  mode: Editor.modes.text,
  lineNumbers: true,
  showAnnotationRuler: true
};

/**
 * The current target and the WebGL Editor front, set by this tool's host.
 */
let gTarget, gFront;

/**
 * Initializes the shader editor controller and views.
 */
function startupShaderEditor() {
  return promise.all([
    EventsHandler.initialize(),
    ShadersListView.initialize(),
    ShadersEditorsView.initialize()
  ]);
}

/**
 * Destroys the shader editor controller and views.
 */
function shutdownShaderEditor() {
  return promise.all([
    EventsHandler.destroy(),
    ShadersListView.destroy(),
    ShadersEditorsView.destroy()
  ]);
}

/**
 * Functions handling target-related lifetime events.
 */
let EventsHandler = {
  /**
   * Listen for events emitted by the current tab target.
   */
  initialize: function() {
    this._onWillNavigate = this._onWillNavigate.bind(this);
    this._onProgramLinked = this._onProgramLinked.bind(this);
    gTarget.on("will-navigate", this._onWillNavigate);
    gFront.on("program-linked", this._onProgramLinked);

  },

  /**
   * Remove events emitted by the current tab target.
   */
  destroy: function() {
    gTarget.off("will-navigate", this._onWillNavigate);
    gFront.off("program-linked", this._onProgramLinked);
  },

  /**
   * Called for each location change in the debugged tab.
   */
  _onWillNavigate: function() {
    gFront.setup();

    ShadersListView.empty();
    ShadersEditorsView.setText({ vs: "", fs: "" });
    $("#reload-notice").hidden = true;
    $("#waiting-notice").hidden = false;
    $("#content").hidden = true;
  },

  /**
   * Called every time a program was linked in the debugged tab.
   */
  _onProgramLinked: function(programActor) {
    $("#waiting-notice").hidden = true;
    $("#reload-notice").hidden = true;
    $("#content").hidden = false;
    ShadersListView.addProgram(programActor);
  }
};

/**
 * Functions handling the sources UI.
 */
let ShadersListView = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this.widget = new SideMenuWidget(this._pane = $("#shaders-pane"), {
      showArrows: true,
      showItemCheckboxes: true
    });

    this._onShaderSelect = this._onShaderSelect.bind(this);
    this._onShaderCheck = this._onShaderCheck.bind(this);
    this._onShaderMouseEnter = this._onShaderMouseEnter.bind(this);
    this._onShaderMouseLeave = this._onShaderMouseLeave.bind(this);

    this.widget.addEventListener("select", this._onShaderSelect, false);
    this.widget.addEventListener("check", this._onShaderCheck, false);
    this.widget.addEventListener("mouseenter", this._onShaderMouseEnter, true);
    this.widget.addEventListener("mouseleave", this._onShaderMouseLeave, true);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    this.widget.removeEventListener("select", this._onShaderSelect, false);
    this.widget.removeEventListener("check", this._onShaderCheck, false);
    this.widget.removeEventListener("mouseenter", this._onShaderMouseEnter, true);
    this.widget.removeEventListener("mouseleave", this._onShaderMouseLeave, true);
  },

  /**
   * Adds a program to this programs container.
   *
   * @param object programActor
   *        The program actor coming from the active thread.
   */
  addProgram: function(programActor) {
    // Currently, there's no good way of differentiating between programs
    // in a way that helps humans. It will be a good idea to implement a
    // standard of allowing debuggees to add some identifiable metadata to their
    // program sources or instances.
    let label = L10N.getFormatStr("shadersList.programLabel", this.itemCount);

    // Append a program item to this container.
    this.push([label, ""], {
      index: -1, /* specifies on which position should the item be appended */
      relaxed: true, /* this container should allow dupes & degenerates */
      attachment: {
        programActor: programActor,
        checkboxState: true,
        checkboxTooltip: L10N.getStr("shadersList.blackboxLabel")
      }
    });

    // Make sure there's always a selected item available.
    if (!this.selectedItem) {
      this.selectedIndex = 0;
    }
  },

  /**
   * The select listener for the sources container.
   */
  _onShaderSelect: function({ detail: sourceItem }) {
    if (!sourceItem) {
      return;
    }
    // The container is not empty and an actual item was selected.
    let attachment = sourceItem.attachment;

    function getShaders() {
      return promise.all([
        attachment.vs || (attachment.vs = attachment.programActor.getVertexShader()),
        attachment.fs || (attachment.fs = attachment.programActor.getFragmentShader())
      ]);
    }
    function getSources([vertexShaderActor, fragmentShaderActor]) {
      return promise.all([
        vertexShaderActor.getText(),
        fragmentShaderActor.getText()
      ]);
    }
    function showSources([vertexShaderText, fragmentShaderText]) {
      ShadersEditorsView.setText({
        vs: vertexShaderText,
        fs: fragmentShaderText
      });
    }

    getShaders().then(getSources).then(showSources).then(null, Cu.reportError);
  },

  /**
   * The check listener for the sources container.
   */
  _onShaderCheck: function({ detail: { checked }, target }) {
    let sourceItem = this.getItemForElement(target);
    let attachment = sourceItem.attachment;
    attachment.isBlackBoxed = !checked;
    attachment.programActor[checked ? "unhighlight" : "highlight"](BLACKBOX_COLOR);
  },

  /**
   * The mouseenter listener for the sources container.
   */
  _onShaderMouseEnter: function(e) {
    let sourceItem = this.getItemForElement(e.target, { noSiblings: true });
    if (sourceItem && !sourceItem.attachment.isBlackBoxed) {
      sourceItem.attachment.programActor.highlight(HIGHLIGHT_COLOR);

      if (e instanceof Event) {
        e.preventDefault();
        e.stopPropagation();
      }
    }
  },

  /**
   * The mouseleave listener for the sources container.
   */
  _onShaderMouseLeave: function(e) {
    let sourceItem = this.getItemForElement(e.target, { noSiblings: true });
    if (sourceItem && !sourceItem.attachment.isBlackBoxed) {
      sourceItem.attachment.programActor.unhighlight();

      if (e instanceof Event) {
        e.preventDefault();
        e.stopPropagation();
      }
    }
  }
});

/**
 * Functions handling the editors displaying the vertex and fragment shaders.
 */
let ShadersEditorsView = {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    XPCOMUtils.defineLazyGetter(this, "_editorPromises", () => new Map());
    this._vsFocused = this._onFocused.bind(this, "vs", "fs");
    this._fsFocused = this._onFocused.bind(this, "fs", "vs");
    this._vsChanged = this._onChanged.bind(this, "vs");
    this._fsChanged = this._onChanged.bind(this, "fs");
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    this._toggleListeners("off");
  },

  /**
   * Sets the text displayed in the vertex and fragment shader editors.
   *
   * @param object sources
   *        An object containing the following properties
   *          - vs: the vertex shader source code
   *          - fs: the fragment shader source code
   */
  setText: function(sources) {
    function setTextAndClearHistory(editor, text) {
      editor.setText(text);
      editor.clearHistory();
    }

    this._toggleListeners("off");
    this._getEditor("vs").then(e => setTextAndClearHistory(e, sources.vs));
    this._getEditor("fs").then(e => setTextAndClearHistory(e, sources.fs));
    this._toggleListeners("on");

    window.emit(EVENTS.SOURCES_SHOWN, sources);
  },

  /**
   * Lazily initializes and returns a promise for an Editor instance.
   *
   * @param string type
   *        Specifies for which shader type should an editor be retrieved,
   *        either are "vs" for a vertex, or "fs" for a fragment shader.
   */
  _getEditor: function(type) {
    if ($("#content").hidden) {
      return promise.reject(null);
    }
    if (this._editorPromises.has(type)) {
      return this._editorPromises.get(type);
    }

    let deferred = promise.defer();
    this._editorPromises.set(type, deferred.promise);

    // Initialize the source editor and store the newly created instance
    // in the ether of a resolved promise's value.
    let parent = $("#" + type +"-editor");
    let editor = new Editor(DEFAULT_EDITOR_CONFIG);
    editor.appendTo(parent).then(() => deferred.resolve(editor));

    return deferred.promise;
  },

  /**
   * Toggles all the event listeners for the editors either on or off.
   *
   * @param string flag
   *        Either "on" to enable the event listeners, "off" to disable them.
   */
  _toggleListeners: function(flag) {
    ["vs", "fs"].forEach(type => {
      this._getEditor(type).then(editor => {
        editor[flag]("focus", this["_" + type + "Focused"]);
        editor[flag]("change", this["_" + type + "Changed"]);
      });
    });
  },

  /**
   * The focus listener for a source editor.
   *
   * @param string focused
   *        The corresponding shader type for the focused editor (e.g. "vs").
   * @param string focused
   *        The corresponding shader type for the other editor (e.g. "fs").
   */
  _onFocused: function(focused, unfocused) {
    $("#" + focused + "-editor-label").setAttribute("selected", "");
    $("#" + unfocused + "-editor-label").removeAttribute("selected");
  },

  /**
   * The change listener for a source editor.
   *
   * @param string type
   *        The corresponding shader type for the focused editor (e.g. "vs").
   */
  _onChanged: function(type) {
    setNamedTimeout("gl-typed", TYPING_MAX_DELAY, () => this._doCompile(type));
  },

  /**
   * Recompiles the source code for the shader being edited.
   * This function is fired at a certain delay after the user stops typing.
   *
   * @param string type
   *        The corresponding shader type for the focused editor (e.g. "vs").
   */
  _doCompile: function(type) {
    Task.spawn(function() {
      let editor = yield this._getEditor(type);
      let shaderActor = yield ShadersListView.selectedAttachment[type];

      try {
        yield shaderActor.compile(editor.getText());
        window.emit(EVENTS.SHADER_COMPILED, null);
        // TODO: remove error gutter markers, after bug 919709 lands.
      } catch (error) {
        window.emit(EVENTS.SHADER_COMPILED, error);
        // TODO: add error gutter markers, after bug 919709 lands.
      }
    }.bind(this));
  }
};

/**
 * Localization convenience methods.
 */
let L10N = new ViewHelpers.L10N(STRINGS_URI);

/**
 * Convenient way of emitting events from the panel window.
 */
EventEmitter.decorate(this);

/**
 * DOM query helper.
 */
function $(selector, target = document) target.querySelector(selector);
