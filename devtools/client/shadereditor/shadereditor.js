/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const {SideMenuWidget} = require("resource://devtools/client/shared/widgets/SideMenuWidget.jsm");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const Services = require("Services");
const EventEmitter = require("devtools/shared/old-event-emitter");
const Tooltip = require("devtools/client/shared/widgets/tooltip/Tooltip");
const Editor = require("devtools/client/sourceeditor/editor");
const {LocalizationHelper} = require("devtools/shared/l10n");
const {extend} = require("devtools/shared/extend");
const {WidgetMethods, setNamedTimeout} =
  require("devtools/client/shared/widgets/view-helpers");
const {Task} = require("devtools/shared/task");

// Use privileged promise in panel documents to prevent having them to freeze
// during toolbox destruction. See bug 1402779.
const Promise = require("Promise");

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When new programs are received from the server.
  NEW_PROGRAM: "ShaderEditor:NewProgram",
  PROGRAMS_ADDED: "ShaderEditor:ProgramsAdded",

  // When the vertex and fragment sources were shown in the editor.
  SOURCES_SHOWN: "ShaderEditor:SourcesShown",

  // When a shader's source was edited and compiled via the editor.
  SHADER_COMPILED: "ShaderEditor:ShaderCompiled",

  // When the UI is reset from tab navigation
  UI_RESET: "ShaderEditor:UIReset",

  // When the editor's error markers are all removed
  EDITOR_ERROR_MARKERS_REMOVED: "ShaderEditor:EditorCleaned"
};
XPCOMUtils.defineConstant(this, "EVENTS", EVENTS);

const STRINGS_URI = "devtools/client/locales/shadereditor.properties";
const HIGHLIGHT_TINT = [1, 0, 0.25, 1]; // rgba
const TYPING_MAX_DELAY = 500; // ms
const SHADERS_AUTOGROW_ITEMS = 4;
const GUTTER_ERROR_PANEL_OFFSET_X = 7; // px
const GUTTER_ERROR_PANEL_DELAY = 100; // ms
const DEFAULT_EDITOR_CONFIG = {
  gutters: ["errors"],
  lineNumbers: true,
  showAnnotationRuler: true
};

/**
 * The current target and the WebGL Editor front, set by this tool's host.
 */
var gToolbox, gTarget, gFront;

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
var EventsHandler = {
  /**
   * Listen for events emitted by the current tab target.
   */
  initialize: function () {
    this._onHostChanged = this._onHostChanged.bind(this);
    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onProgramLinked = this._onProgramLinked.bind(this);
    this._onProgramsAdded = this._onProgramsAdded.bind(this);
    gToolbox.on("host-changed", this._onHostChanged);
    gTarget.on("will-navigate", this._onTabNavigated);
    gTarget.on("navigate", this._onTabNavigated);
    gFront.on("program-linked", this._onProgramLinked);
    this.reloadButton = $("#requests-menu-reload-notice-button");
    this.reloadButton.addEventListener("command", this._onReloadCommand);
  },

  /**
   * Remove events emitted by the current tab target.
   */
  destroy: function () {
    gToolbox.off("host-changed", this._onHostChanged);
    gTarget.off("will-navigate", this._onTabNavigated);
    gTarget.off("navigate", this._onTabNavigated);
    gFront.off("program-linked", this._onProgramLinked);
    this.reloadButton.removeEventListener("command", this._onReloadCommand);
  },

  /**
   * Handles a command event on reload button
   */
  _onReloadCommand() {
    gFront.setup({ reload: true });
  },

  /**
   * Handles a host change event on the parent toolbox.
   */
  _onHostChanged: function () {
    if (gToolbox.hostType == "side") {
      $("#shaders-pane").removeAttribute("height");
    }
  },

  /**
   * Called for each location change in the debugged tab.
   */
  _onTabNavigated: function (event, {isFrameSwitching}) {
    switch (event) {
      case "will-navigate": {
        // Make sure the backend is prepared to handle WebGL contexts.
        if (!isFrameSwitching) {
          gFront.setup({ reload: false });
        }

        // Reset UI.
        ShadersListView.empty();
        // When switching to an iframe, ensure displaying the reload button.
        // As the document has already been loaded without being hooked.
        if (isFrameSwitching) {
          $("#reload-notice").hidden = false;
          $("#waiting-notice").hidden = true;
        } else {
          $("#reload-notice").hidden = true;
          $("#waiting-notice").hidden = false;
        }

        $("#content").hidden = true;
        window.emit(EVENTS.UI_RESET);

        break;
      }
      case "navigate": {
        // Manually retrieve the list of program actors known to the server,
        // because the backend won't emit "program-linked" notifications
        // in the case of a bfcache navigation (since no new programs are
        // actually linked).
        gFront.getPrograms().then(this._onProgramsAdded);
        break;
      }
    }
  },

  /**
   * Called every time a program was linked in the debugged tab.
   */
  _onProgramLinked: function (programActor) {
    this._addProgram(programActor);
    window.emit(EVENTS.NEW_PROGRAM);
  },

  /**
   * Callback for the front's getPrograms() method.
   */
  _onProgramsAdded: function (programActors) {
    programActors.forEach(this._addProgram);
    window.emit(EVENTS.PROGRAMS_ADDED);
  },

  /**
   * Adds a program to the shaders list and unhides any modal notices.
   */
  _addProgram: function (programActor) {
    $("#waiting-notice").hidden = true;
    $("#reload-notice").hidden = true;
    $("#content").hidden = false;
    ShadersListView.addProgram(programActor);
  }
};

/**
 * Functions handling the sources UI.
 */
var ShadersListView = extend(WidgetMethods, {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function () {
    this.widget = new SideMenuWidget(this._pane = $("#shaders-pane"), {
      showArrows: true,
      showItemCheckboxes: true
    });

    this._onProgramSelect = this._onProgramSelect.bind(this);
    this._onProgramCheck = this._onProgramCheck.bind(this);
    this._onProgramMouseOver = this._onProgramMouseOver.bind(this);
    this._onProgramMouseOut = this._onProgramMouseOut.bind(this);

    this.widget.addEventListener("select", this._onProgramSelect);
    this.widget.addEventListener("check", this._onProgramCheck);
    this.widget.addEventListener("mouseover", this._onProgramMouseOver, true);
    this.widget.addEventListener("mouseout", this._onProgramMouseOut, true);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function () {
    this.widget.removeEventListener("select", this._onProgramSelect);
    this.widget.removeEventListener("check", this._onProgramCheck);
    this.widget.removeEventListener("mouseover", this._onProgramMouseOver, true);
    this.widget.removeEventListener("mouseout", this._onProgramMouseOut, true);
  },

  /**
   * Adds a program to this programs container.
   *
   * @param object programActor
   *        The program actor coming from the active thread.
   */
  addProgram: function (programActor) {
    if (this.hasProgram(programActor)) {
      return;
    }

    // Currently, there's no good way of differentiating between programs
    // in a way that helps humans. It will be a good idea to implement a
    // standard of allowing debuggees to add some identifiable metadata to their
    // program sources or instances.
    let label = L10N.getFormatStr("shadersList.programLabel", this.itemCount);
    let contents = document.createElement("label");
    contents.className = "plain program-item";
    contents.setAttribute("value", label);
    contents.setAttribute("crop", "start");
    contents.setAttribute("flex", "1");

    // Append a program item to this container.
    this.push([contents], {
      index: -1, /* specifies on which position should the item be appended */
      attachment: {
        label: label,
        programActor: programActor,
        checkboxState: true,
        checkboxTooltip: L10N.getStr("shadersList.blackboxLabel")
      }
    });

    // Make sure there's always a selected item available.
    if (!this.selectedItem) {
      this.selectedIndex = 0;
    }

    // Prevent this container from growing indefinitely in height when the
    // toolbox is docked to the side.
    if (gToolbox.hostType == "side" && this.itemCount == SHADERS_AUTOGROW_ITEMS) {
      this._pane.setAttribute("height", this._pane.getBoundingClientRect().height);
    }
  },

  /**
   * Returns whether a program was already added to this programs container.
   *
   * @param object programActor
   *        The program actor coming from the active thread.
   * @param boolean
   *        True if the program was added, false otherwise.
   */
  hasProgram: function (programActor) {
    return !!this.attachments.filter(e => e.programActor == programActor).length;
  },

  /**
   * The select listener for the programs container.
   */
  _onProgramSelect: function ({ detail: sourceItem }) {
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
      return ShadersEditorsView.setText({
        vs: vertexShaderText,
        fs: fragmentShaderText
      });
    }

    getShaders()
      .then(getSources)
      .then(showSources)
      .catch(console.error);
  },

  /**
   * The check listener for the programs container.
   */
  _onProgramCheck: function ({ detail: { checked }, target }) {
    let sourceItem = this.getItemForElement(target);
    let attachment = sourceItem.attachment;
    attachment.isBlackBoxed = !checked;
    attachment.programActor[checked ? "unblackbox" : "blackbox"]();
  },

  /**
   * The mouseover listener for the programs container.
   */
  _onProgramMouseOver: function (e) {
    let sourceItem = this.getItemForElement(e.target, { noSiblings: true });
    if (sourceItem && !sourceItem.attachment.isBlackBoxed) {
      sourceItem.attachment.programActor.highlight(HIGHLIGHT_TINT);

      if (e instanceof Event) {
        e.preventDefault();
        e.stopPropagation();
      }
    }
  },

  /**
   * The mouseout listener for the programs container.
   */
  _onProgramMouseOut: function (e) {
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
var ShadersEditorsView = {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function () {
    XPCOMUtils.defineLazyGetter(this, "_editorPromises", () => new Map());
    this._vsFocused = this._onFocused.bind(this, "vs", "fs");
    this._fsFocused = this._onFocused.bind(this, "fs", "vs");
    this._vsChanged = this._onChanged.bind(this, "vs");
    this._fsChanged = this._onChanged.bind(this, "fs");
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: Task.async(function* () {
    this._destroyed = true;
    yield this._toggleListeners("off");
    for (let p of this._editorPromises.values()) {
      let editor = yield p;
      editor.destroy();
    }
  }),

  /**
   * Sets the text displayed in the vertex and fragment shader editors.
   *
   * @param object sources
   *        An object containing the following properties
   *          - vs: the vertex shader source code
   *          - fs: the fragment shader source code
   * @return object
   *        A promise resolving upon completion of text setting.
   */
  setText: function (sources) {
    let view = this;
    function setTextAndClearHistory(editor, text) {
      editor.setText(text);
      editor.clearHistory();
    }

    return Task.spawn(function* () {
      yield view._toggleListeners("off");
      yield promise.all([
        view._getEditor("vs").then(e => setTextAndClearHistory(e, sources.vs)),
        view._getEditor("fs").then(e => setTextAndClearHistory(e, sources.fs))
      ]);
      yield view._toggleListeners("on");
    }).then(() => window.emit(EVENTS.SOURCES_SHOWN, sources));
  },

  /**
   * Lazily initializes and returns a promise for an Editor instance.
   *
   * @param string type
   *        Specifies for which shader type should an editor be retrieved,
   *        either are "vs" for a vertex, or "fs" for a fragment shader.
   * @return object
   *        Returns a promise that resolves to an editor instance
   */
  _getEditor: function (type) {
    if (this._editorPromises.has(type)) {
      return this._editorPromises.get(type);
    }

    let deferred = defer();
    this._editorPromises.set(type, deferred.promise);

    // Initialize the source editor and store the newly created instance
    // in the ether of a resolved promise's value.
    let parent = $("#" + type + "-editor");
    let editor = new Editor(DEFAULT_EDITOR_CONFIG);
    editor.config.mode = Editor.modes[type];

    if (this._destroyed) {
      deferred.resolve(editor);
    } else {
      editor.appendTo(parent).then(() => deferred.resolve(editor));
    }

    return deferred.promise;
  },

  /**
   * Toggles all the event listeners for the editors either on or off.
   *
   * @param string flag
   *        Either "on" to enable the event listeners, "off" to disable them.
   * @return object
   *        A promise resolving upon completion of toggling the listeners.
   */
  _toggleListeners: function (flag) {
    return promise.all(["vs", "fs"].map(type => {
      return this._getEditor(type).then(editor => {
        editor[flag]("focus", this["_" + type + "Focused"]);
        editor[flag]("change", this["_" + type + "Changed"]);
      });
    }));
  },

  /**
   * The focus listener for a source editor.
   *
   * @param string focused
   *        The corresponding shader type for the focused editor (e.g. "vs").
   * @param string focused
   *        The corresponding shader type for the other editor (e.g. "fs").
   */
  _onFocused: function (focused, unfocused) {
    $("#" + focused + "-editor-label").setAttribute("selected", "");
    $("#" + unfocused + "-editor-label").removeAttribute("selected");
  },

  /**
   * The change listener for a source editor.
   *
   * @param string type
   *        The corresponding shader type for the focused editor (e.g. "vs").
   */
  _onChanged: function (type) {
    setNamedTimeout("gl-typed", TYPING_MAX_DELAY, () => this._doCompile(type));

    // Remove all the gutter markers and line classes from the editor.
    this._cleanEditor(type);
  },

  /**
   * Recompiles the source code for the shader being edited.
   * This function is fired at a certain delay after the user stops typing.
   *
   * @param string type
   *        The corresponding shader type for the focused editor (e.g. "vs").
   */
  _doCompile: function (type) {
    Task.spawn(function* () {
      let editor = yield this._getEditor(type);
      let shaderActor = yield ShadersListView.selectedAttachment[type];

      try {
        yield shaderActor.compile(editor.getText());
        this._onSuccessfulCompilation();
      } catch (e) {
        this._onFailedCompilation(type, editor, e);
      }
    }.bind(this));
  },

  /**
   * Called uppon a successful shader compilation.
   */
  _onSuccessfulCompilation: function () {
    // Signal that the shader was compiled successfully.
    window.emit(EVENTS.SHADER_COMPILED, null);
  },

  /**
   * Called uppon an unsuccessful shader compilation.
   */
  _onFailedCompilation: function (type, editor, errors) {
    let lineCount = editor.lineCount();
    let currentLine = editor.getCursor().line;
    let listeners = { mouseover: this._onMarkerMouseOver };

    function matchLinesAndMessages(string) {
      return {
        // First number that is not equal to 0.
        lineMatch: string.match(/\d{2,}|[1-9]/),
        // The string after all the numbers, semicolons and spaces.
        textMatch: string.match(/[^\s\d:][^\r\n|]*/)
      };
    }
    function discardInvalidMatches(e) {
      // Discard empty line and text matches.
      return e.lineMatch && e.textMatch;
    }
    function sanitizeValidMatches(e) {
      return {
        // Drivers might yield confusing line numbers under some obscure
        // circumstances. Don't throw the errors away in those cases,
        // just display them on the currently edited line.
        line: e.lineMatch[0] > lineCount ? currentLine : e.lineMatch[0] - 1,
        // Trim whitespace from the beginning and the end of the message,
        // and replace all other occurences of double spaces to a single space.
        text: e.textMatch[0].trim().replace(/\s{2,}/g, " ")
      };
    }
    function sortByLine(first, second) {
      // Sort all the errors ascending by their corresponding line number.
      return first.line > second.line ? 1 : -1;
    }
    function groupSameLineMessages(accumulator, current) {
      // Group errors corresponding to the same line number to a single object.
      let previous = accumulator[accumulator.length - 1];
      if (!previous || previous.line != current.line) {
        return [...accumulator, {
          line: current.line,
          messages: [current.text]
        }];
      } else {
        previous.messages.push(current.text);
        return accumulator;
      }
    }
    function displayErrors({ line, messages }) {
      // Add gutter markers and line classes for every error in the source.
      editor.addMarker(line, "errors", "error");
      editor.setMarkerListeners(line, "errors", "error", listeners, messages);
      editor.addLineClass(line, "error-line");
    }

    (this._errors[type] = errors.link
      .split("ERROR")
      .map(matchLinesAndMessages)
      .filter(discardInvalidMatches)
      .map(sanitizeValidMatches)
      .sort(sortByLine)
      .reduce(groupSameLineMessages, []))
      .forEach(displayErrors);

    // Signal that the shader wasn't compiled successfully.
    window.emit(EVENTS.SHADER_COMPILED, errors);
  },

  /**
   * Event listener for the 'mouseover' event on a marker in the editor gutter.
   */
  _onMarkerMouseOver: function (line, node, messages) {
    if (node._markerErrorsTooltip) {
      return;
    }

    let tooltip = node._markerErrorsTooltip = new Tooltip(document);
    tooltip.defaultOffsetX = GUTTER_ERROR_PANEL_OFFSET_X;
    tooltip.setTextContent({ messages: messages });
    tooltip.startTogglingOnHover(node, () => true, {
      toggleDelay: GUTTER_ERROR_PANEL_DELAY
    });
  },

  /**
   * Removes all the gutter markers and line classes from the editor.
   */
  _cleanEditor: function (type) {
    this._getEditor(type).then(editor => {
      editor.removeAllMarkers("errors");
      this._errors[type].forEach(e => editor.removeLineClass(e.line));
      this._errors[type].length = 0;
      window.emit(EVENTS.EDITOR_ERROR_MARKERS_REMOVED);
    });
  },

  _errors: {
    vs: [],
    fs: []
  }
};

/**
 * Localization convenience methods.
 */
var L10N = new LocalizationHelper(STRINGS_URI);

/**
 * Convenient way of emitting events from the panel window.
 */
EventEmitter.decorate(this);

/**
 * DOM query helper.
 */
var $ = (selector, target = document) => target.querySelector(selector);
