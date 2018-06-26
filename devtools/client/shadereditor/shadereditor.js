/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const {SideMenuWidget} = require("devtools/client/shared/widgets/SideMenuWidget.jsm");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const {Task} = require("devtools/shared/task");
const EventEmitter = require("devtools/shared/event-emitter");
const Tooltip = require("devtools/client/shared/widgets/tooltip/Tooltip");
const Editor = require("devtools/client/sourceeditor/editor");
const {LocalizationHelper} = require("devtools/shared/l10n");
const {extend} = require("devtools/shared/extend");
const {WidgetMethods, setNamedTimeout} =
  require("devtools/client/shared/widgets/view-helpers");

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
exports.EVENTS = EVENTS;

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
 * Functions handling target-related lifetime events.
 */
class EventsHandler {
  /**
   * Listen for events emitted by the current tab target.
   */
  initialize(panel, toolbox, target, front, shadersListView) {
    this.panel = panel;
    this.toolbox = toolbox;
    this.target = target;
    this.front = front;
    this.shadersListView = shadersListView;

    this._onHostChanged = this._onHostChanged.bind(this);
    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onTabWillNavigate = this._onTabWillNavigate.bind(this);
    this._onProgramLinked = this._onProgramLinked.bind(this);
    this._onProgramsAdded = this._onProgramsAdded.bind(this);

    this.toolbox.on("host-changed", this._onHostChanged);
    this.target.on("will-navigate", this._onTabWillNavigate);
    this.target.on("navigate", this._onTabNavigated);
    this.front.on("program-linked", this._onProgramLinked);
    this.reloadButton = $("#requests-menu-reload-notice-button");
    this._onReloadCommand = this._onReloadCommand.bind(this);
    this.reloadButton.addEventListener("command", this._onReloadCommand);
  }

  /**
   * Remove events emitted by the current tab target.
   */
  destroy() {
    this.toolbox.off("host-changed", this._onHostChanged);
    this.target.off("will-navigate", this._onTabWillNavigate);
    this.target.off("navigate", this._onTabNavigated);
    this.front.off("program-linked", this._onProgramLinked);
    this.reloadButton.removeEventListener("command", this._onReloadCommand);
  }

  /**
   * Handles a command event on reload button
   */
  _onReloadCommand() {
    this.front.setup({ reload: true });
  }

  /**
   * Handles a host change event on the parent toolbox.
   */
  _onHostChanged() {
    if (this.toolbox.hostType == "right" || this.toolbox.hostType == "left") {
      $("#shaders-pane").removeAttribute("height");
    }
  }

  _onTabWillNavigate({isFrameSwitching}) {
    // Make sure the backend is prepared to handle WebGL contexts.
    if (!isFrameSwitching) {
      this.front.setup({ reload: false });
    }

    // Reset UI.
    this.shadersListView.empty();
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
    this.panel.emit(EVENTS.UI_RESET);
  }

  /**
   * Called for each location change in the debugged tab.
   */
  _onTabNavigated() {
    // Manually retrieve the list of program actors known to the server,
    // because the backend won't emit "program-linked" notifications
    // in the case of a bfcache navigation (since no new programs are
    // actually linked).
    this.front.getPrograms().then(this._onProgramsAdded);
  }

  /**
   * Called every time a program was linked in the debugged tab.
   */
  _onProgramLinked(programActor) {
    this._addProgram(programActor);
    this.panel.emit(EVENTS.NEW_PROGRAM);
  }

  /**
   * Callback for the front's getPrograms() method.
   */
  _onProgramsAdded(programActors) {
    programActors.forEach(this._addProgram.bind(this));
    this.panel.emit(EVENTS.PROGRAMS_ADDED);
  }

  /**
   * Adds a program to the shaders list and unhides any modal notices.
   */
  _addProgram(programActor) {
    $("#waiting-notice").hidden = true;
    $("#reload-notice").hidden = true;
    $("#content").hidden = false;
    this.shadersListView.addProgram(programActor);
  }
}
exports.EventsHandler = EventsHandler;

/**
 * Functions handling the sources UI.
 */
function WidgetMethodsClass() {
}
WidgetMethodsClass.prototype = WidgetMethods;
class ShadersListView extends WidgetMethodsClass {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize(toolbox, shadersEditorsView) {
    this.toolbox = toolbox;
    this.shadersEditorsView = shadersEditorsView;
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
  }

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy() {
    this.widget.removeEventListener("select", this._onProgramSelect);
    this.widget.removeEventListener("check", this._onProgramCheck);
    this.widget.removeEventListener("mouseover", this._onProgramMouseOver, true);
    this.widget.removeEventListener("mouseout", this._onProgramMouseOut, true);
  }

  /**
   * Adds a program to this programs container.
   *
   * @param object programActor
   *        The program actor coming from the active thread.
   */
  addProgram(programActor) {
    if (this.hasProgram(programActor)) {
      return;
    }

    // Currently, there's no good way of differentiating between programs
    // in a way that helps humans. It will be a good idea to implement a
    // standard of allowing debuggees to add some identifiable metadata to their
    // program sources or instances.
    const label = L10N.getFormatStr("shadersList.programLabel", this.itemCount);
    const contents = document.createElement("label");
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
    if ((this.toolbox.hostType == "left" || this.toolbox.hostType == "right") &&
        this.itemCount == SHADERS_AUTOGROW_ITEMS) {
      this._pane.setAttribute("height", this._pane.getBoundingClientRect().height);
    }
  }

  /**
   * Returns whether a program was already added to this programs container.
   *
   * @param object programActor
   *        The program actor coming from the active thread.
   * @param boolean
   *        True if the program was added, false otherwise.
   */
  hasProgram(programActor) {
    return !!this.attachments.filter(e => e.programActor == programActor).length;
  }

  /**
   * The select listener for the programs container.
   */
  _onProgramSelect({ detail: sourceItem }) {
    if (!sourceItem) {
      return;
    }
    // The container is not empty and an actual item was selected.
    const attachment = sourceItem.attachment;

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
    const showSources = ([vertexShaderText, fragmentShaderText]) => {
      return this.shadersEditorsView.setText({
        vs: vertexShaderText,
        fs: fragmentShaderText
      });
    };

    getShaders()
      .then(getSources)
      .then(showSources)
      .catch(console.error);
  }

  /**
   * The check listener for the programs container.
   */
  _onProgramCheck({ detail: { checked }, target }) {
    const sourceItem = this.getItemForElement(target);
    const attachment = sourceItem.attachment;
    attachment.isBlackBoxed = !checked;
    attachment.programActor[checked ? "unblackbox" : "blackbox"]();
  }

  /**
   * The mouseover listener for the programs container.
   */
  _onProgramMouseOver(e) {
    const sourceItem = this.getItemForElement(e.target, { noSiblings: true });
    if (sourceItem && !sourceItem.attachment.isBlackBoxed) {
      sourceItem.attachment.programActor.highlight(HIGHLIGHT_TINT);

      if (e instanceof Event) {
        e.preventDefault();
        e.stopPropagation();
      }
    }
  }

  /**
   * The mouseout listener for the programs container.
   */
  _onProgramMouseOut(e) {
    const sourceItem = this.getItemForElement(e.target, { noSiblings: true });
    if (sourceItem && !sourceItem.attachment.isBlackBoxed) {
      sourceItem.attachment.programActor.unhighlight();

      if (e instanceof Event) {
        e.preventDefault();
        e.stopPropagation();
      }
    }
  }
}
exports.ShadersListView = ShadersListView;

/**
 * Functions handling the editors displaying the vertex and fragment shaders.
 */
class ShadersEditorsView {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize(panel, shadersListView) {
    this.panel = panel;
    this.shadersListView = shadersListView;
    XPCOMUtils.defineLazyGetter(this, "_editorPromises", () => new Map());
    this._vsFocused = this._onFocused.bind(this, "vs", "fs");
    this._fsFocused = this._onFocused.bind(this, "fs", "vs");
    this._vsChanged = this._onChanged.bind(this, "vs");
    this._fsChanged = this._onChanged.bind(this, "fs");

    this._errors = {
      vs: [],
      fs: []
    };
  }

  /**
   * Destruction function, called when the tool is closed.
   */
  async destroy() {
    this._destroyed = true;
    await this._toggleListeners("off");
    for (const p of this._editorPromises.values()) {
      const editor = await p;
      editor.destroy();
    }
  }

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
  setText(sources) {
    const view = this;
    function setTextAndClearHistory(editor, text) {
      editor.setText(text);
      editor.clearHistory();
    }

    return (async function() {
      await view._toggleListeners("off");
      await promise.all([
        view._getEditor("vs").then(e => setTextAndClearHistory(e, sources.vs)),
        view._getEditor("fs").then(e => setTextAndClearHistory(e, sources.fs))
      ]);
      await view._toggleListeners("on");
    })().then(() => this.panel.emit(EVENTS.SOURCES_SHOWN, sources));
  }

  /**
   * Lazily initializes and returns a promise for an Editor instance.
   *
   * @param string type
   *        Specifies for which shader type should an editor be retrieved,
   *        either are "vs" for a vertex, or "fs" for a fragment shader.
   * @return object
   *        Returns a promise that resolves to an editor instance
   */
  _getEditor(type) {
    if (this._editorPromises.has(type)) {
      return this._editorPromises.get(type);
    }

    const deferred = defer();
    this._editorPromises.set(type, deferred.promise);

    // Initialize the source editor and store the newly created instance
    // in the ether of a resolved promise's value.
    const parent = $("#" + type + "-editor");
    const editor = new Editor(DEFAULT_EDITOR_CONFIG);
    editor.config.mode = Editor.modes[type];

    if (this._destroyed) {
      deferred.resolve(editor);
    } else {
      editor.appendTo(parent).then(() => deferred.resolve(editor));
    }

    return deferred.promise;
  }

  /**
   * Toggles all the event listeners for the editors either on or off.
   *
   * @param string flag
   *        Either "on" to enable the event listeners, "off" to disable them.
   * @return object
   *        A promise resolving upon completion of toggling the listeners.
   */
  _toggleListeners(flag) {
    return promise.all(["vs", "fs"].map(type => {
      return this._getEditor(type).then(editor => {
        editor[flag]("focus", this["_" + type + "Focused"]);
        editor[flag]("change", this["_" + type + "Changed"]);
      });
    }));
  }

  /**
   * The focus listener for a source editor.
   *
   * @param string focused
   *        The corresponding shader type for the focused editor (e.g. "vs").
   * @param string focused
   *        The corresponding shader type for the other editor (e.g. "fs").
   */
  _onFocused(focused, unfocused) {
    $("#" + focused + "-editor-label").setAttribute("selected", "");
    $("#" + unfocused + "-editor-label").removeAttribute("selected");
  }

  /**
   * The change listener for a source editor.
   *
   * @param string type
   *        The corresponding shader type for the focused editor (e.g. "vs").
   */
  _onChanged(type) {
    setNamedTimeout("gl-typed", TYPING_MAX_DELAY, () => this._doCompile(type));

    // Remove all the gutter markers and line classes from the editor.
    this._cleanEditor(type);
  }

  /**
   * Recompiles the source code for the shader being edited.
   * This function is fired at a certain delay after the user stops typing.
   *
   * @param string type
   *        The corresponding shader type for the focused editor (e.g. "vs").
   */
  _doCompile(type) {
    (async function() {
      const editor = await this._getEditor(type);
      const shaderActor = await this.shadersListView.selectedAttachment[type];

      try {
        await shaderActor.compile(editor.getText());
        this._onSuccessfulCompilation();
      } catch (e) {
        this._onFailedCompilation(type, editor, e);
      }
    }.bind(this))();
  }

  /**
   * Called uppon a successful shader compilation.
   */
  _onSuccessfulCompilation() {
    // Signal that the shader was compiled successfully.
    this.panel.emit(EVENTS.SHADER_COMPILED, null);
  }

  /**
   * Called uppon an unsuccessful shader compilation.
   */
  _onFailedCompilation(type, editor, errors) {
    const lineCount = editor.lineCount();
    const currentLine = editor.getCursor().line;
    const listeners = { mouseover: this._onMarkerMouseOver };

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
      const previous = accumulator[accumulator.length - 1];
      if (!previous || previous.line != current.line) {
        return [...accumulator, {
          line: current.line,
          messages: [current.text]
        }];
      }
      previous.messages.push(current.text);
      return accumulator;
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
    this.panel.emit(EVENTS.SHADER_COMPILED, errors);
  }

  /**
   * Event listener for the 'mouseover' event on a marker in the editor gutter.
   */
  _onMarkerMouseOver(line, node, messages) {
    if (node._markerErrorsTooltip) {
      return;
    }

    const tooltip = node._markerErrorsTooltip = new Tooltip(document);
    tooltip.defaultOffsetX = GUTTER_ERROR_PANEL_OFFSET_X;
    tooltip.setTextContent({ messages: messages });
    tooltip.startTogglingOnHover(node, () => true, {
      toggleDelay: GUTTER_ERROR_PANEL_DELAY
    });
  }

  /**
   * Removes all the gutter markers and line classes from the editor.
   */
  _cleanEditor(type) {
    this._getEditor(type).then(editor => {
      editor.removeAllMarkers("errors");
      this._errors[type].forEach(e => editor.removeLineClass(e.line));
      this._errors[type].length = 0;
      this.panel.emit(EVENTS.EDITOR_ERROR_MARKERS_REMOVED);
    });
  }
}
exports.ShadersEditorsView = ShadersEditorsView;

/**
 * Localization convenience methods.
 */
var L10N = new LocalizationHelper(STRINGS_URI);
exports.L10N = L10N;

/**
 * DOM query helper.
 */
var $ = (selector, target = document) => target.querySelector(selector);
exports.$ = $;
