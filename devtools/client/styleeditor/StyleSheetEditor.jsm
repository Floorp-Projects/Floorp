/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["StyleSheetEditor"];

const { require, loader } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const Editor = require("devtools/client/shared/sourceeditor/editor");
const {
  shortSource,
  prettifyCSS,
} = require("devtools/shared/inspector/css-logic");
const { throttle } = require("devtools/shared/throttle");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

const lazy = {};

loader.lazyGetter(lazy, "BufferStream", () => {
  const { CC } = require("chrome");

  return CC(
    "@mozilla.org/io/arraybuffer-input-stream;1",
    "nsIArrayBufferInputStream",
    "setData"
  );
});

loader.lazyRequireGetter(
  lazy,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm",
  true
);
loader.lazyRequireGetter(
  lazy,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm",
  true
);

const {
  getString,
  showFilePicker,
} = require("resource://devtools/client/styleeditor/StyleEditorUtil.jsm");

const LOAD_ERROR = "error-load";
const SAVE_ERROR = "error-save";
const SELECTOR_HIGHLIGHTER_TYPE = "SelectorHighlighter";

// max update frequency in ms (avoid potential typing lag and/or flicker)
// @see StyleEditor.updateStylesheet
const UPDATE_STYLESHEET_DELAY = 500;

// Pref which decides if CSS autocompletion is enabled in Style Editor or not.
const AUTOCOMPLETION_PREF = "devtools.styleeditor.autocompletion-enabled";

// Pref which decides whether updates to the stylesheet use transitions
const TRANSITION_PREF = "devtools.styleeditor.transitions";

// How long to wait to update linked CSS file after original source was saved
// to disk. Time in ms.
const CHECK_LINKED_SHEET_DELAY = 500;

// How many times to check for linked file changes
const MAX_CHECK_COUNT = 10;

// How much time should the mouse be still before the selector at that position
// gets highlighted?
const SELECTOR_HIGHLIGHT_TIMEOUT = 500;

// Minimum delay between firing two media-rules-changed events.
const EMIT_MEDIA_RULES_THROTTLING = 500;

const STYLE_SHEET_UPDATE_CAUSED_BY_STYLE_EDITOR = "styleeditor";

/**
 * StyleSheetEditor controls the editor linked to a particular StyleSheet
 * object.
 *
 * Emits events:
 *   'property-change': A property on the underlying stylesheet has changed
 *   'source-editor-load': The source editor for this editor has been loaded
 *   'error': An error has occured
 *
 * @param  {Resource} resource
 *         The STYLESHEET resource which is received from resource command.
 * @param {DOMWindow}  win
 *        panel window for style editor
 * @param {Number} styleSheetFriendlyIndex
 *        Optional Integer representing the index of the current stylesheet
 *        among all stylesheets of its type (inline or user-created)
 */
function StyleSheetEditor(resource, win, styleSheetFriendlyIndex) {
  EventEmitter.decorate(this);

  this._resource = resource;
  this._inputElement = null;
  this.sourceEditor = null;
  this._window = win;
  this._isNew = this.styleSheet.isNew;
  this.styleSheetFriendlyIndex = styleSheetFriendlyIndex;

  // True when we've just set the editor text based on a style-applied
  // event from the StyleSheetActor.
  this._justSetText = false;

  // state to use when inputElement attaches
  this._state = {
    text: "",
    selection: {
      start: { line: 0, ch: 0 },
      end: { line: 0, ch: 0 },
    },
  };

  this._styleSheetFilePath = null;
  if (
    this.styleSheet.href &&
    Services.io.extractScheme(this.styleSheet.href) == "file"
  ) {
    this._styleSheetFilePath = this.styleSheet.href;
  }

  this.onPropertyChange = this.onPropertyChange.bind(this);
  this.onMediaRulesChanged = this.onMediaRulesChanged.bind(this);
  this.checkLinkedFileForChanges = this.checkLinkedFileForChanges.bind(this);
  this.markLinkedFileBroken = this.markLinkedFileBroken.bind(this);
  this.saveToFile = this.saveToFile.bind(this);
  this.updateStyleSheet = this.updateStyleSheet.bind(this);
  this._updateStyleSheet = this._updateStyleSheet.bind(this);
  this._onMouseMove = this._onMouseMove.bind(this);

  this._focusOnSourceEditorReady = false;
  this.savedFile = this.styleSheet.file;
  this.linkCSSFile();

  this.emitMediaRulesChanged = throttle(
    this.emitMediaRulesChanged,
    EMIT_MEDIA_RULES_THROTTLING,
    this
  );

  this.mediaRules = [];
}

StyleSheetEditor.prototype = {
  get resourceId() {
    return this._resource.resourceId;
  },

  get styleSheet() {
    return this._resource;
  },

  /**
   * Whether there are unsaved changes in the editor
   */
  get unsaved() {
    return this.sourceEditor && !this.sourceEditor.isClean();
  },

  /**
   * Whether the editor is for a stylesheet created by the user
   * through the style editor UI.
   */
  get isNew() {
    return this._isNew;
  },

  /**
   * The style sheet or the generated style sheet for this source if it's an
   * original source.
   */
  get cssSheet() {
    if (this.styleSheet.isOriginalSource) {
      return this.styleSheet.relatedStyleSheet;
    }
    return this.styleSheet;
  },

  get savedFile() {
    return this._savedFile;
  },

  set savedFile(name) {
    this._savedFile = name;

    this.linkCSSFile();
  },

  /**
   * Get a user-friendly name for the style sheet.
   *
   * @return string
   */
  get friendlyName() {
    if (this.savedFile) {
      return this.savedFile.leafName;
    }

    if (this._isNew) {
      const index = this.styleSheetFriendlyIndex + 1 || 0;
      return getString("newStyleSheet", index);
    }

    if (!this.styleSheet.href) {
      // TODO(bug 176993): Probably a different index + string for
      // constructable stylesheets, they can't be meaningfully edited right now
      // because we don't have their original text.
      const index = this.styleSheetFriendlyIndex + 1 || 0;
      return getString("inlineStyleSheet", index);
    }

    if (!this._friendlyName) {
      this._friendlyName = shortSource(this.styleSheet);
      try {
        this._friendlyName = decodeURI(this._friendlyName);
      } catch (ex) {
        // Ignore.
      }
    }
    return this._friendlyName;
  },

  /**
   * Check if transitions are enabled for style changes.
   *
   * @return Boolean
   */
  get transitionsEnabled() {
    return Services.prefs.getBoolPref(TRANSITION_PREF);
  },

  /**
   * If this is an original source, get the path of the CSS file it generated.
   */
  linkCSSFile: function() {
    if (!this.styleSheet.isOriginalSource) {
      return;
    }

    const relatedSheet = this.styleSheet.relatedStyleSheet;
    if (!relatedSheet || !relatedSheet.href) {
      return;
    }

    let path;
    const href = removeQuery(relatedSheet.href);
    const uri = lazy.NetUtil.newURI(href);

    if (uri.scheme == "file") {
      const file = uri.QueryInterface(Ci.nsIFileURL).file;
      path = file.path;
    } else if (this.savedFile) {
      const origHref = removeQuery(this.styleSheet.href);
      const origUri = lazy.NetUtil.newURI(origHref);
      path = findLinkedFilePath(uri, origUri, this.savedFile);
    } else {
      // we can't determine path to generated file on disk
      return;
    }

    if (this.linkedCSSFile == path) {
      return;
    }

    this.linkedCSSFile = path;

    this.linkedCSSFileError = null;

    // save last file change time so we can compare when we check for changes.
    IOUtils.stat(path).then(info => {
      this._fileModDate = info.lastModified;
    }, this.markLinkedFileBroken);

    this.emit("linked-css-file");
  },

  /**
   * A helper function that fetches the source text from the style
   * sheet.  The text is possibly prettified using prettifyCSS.  This
   * also sets |this._state.text| to the new text.
   *
   * @return {Promise} a promise that resolves to the new text
   */
  async _getSourceTextAndPrettify() {
    const styleSheetsFront = await this._getStyleSheetsFront();

    let longStr = null;
    if (this.styleSheet.isOriginalSource) {
      // If the stylesheet is OriginalSource, we should get the texts from SourceMapService.
      // So, for now, we use OriginalSource.getText() as it is.
      longStr = await this.styleSheet.getText();
    } else {
      longStr = await styleSheetsFront.getText(this.resourceId);
    }

    let source = await longStr.string();
    const ruleCount = this.styleSheet.ruleCount;
    if (!this.styleSheet.isOriginalSource) {
      const { result, mappings } = prettifyCSS(source, ruleCount);
      source = result;
      // Store the list of objects with mappings between CSS token positions from the
      // original source to the prettified source. These will be used when requested to
      // jump to a specific position within the editor.
      this._mappings = mappings;
    }

    this._state.text = source;
    return source;
  },

  /**
   * Start fetching the full text source for this editor's sheet.
   *
   * @return {Promise}
   *         A promise that'll resolve with the source text once the source
   *         has been loaded or reject on unexpected error.
   */
  fetchSource: function() {
    return this._getSourceTextAndPrettify()
      .then(source => {
        this.sourceLoaded = true;
        return source;
      })
      .catch(e => {
        if (this._isDestroyed) {
          console.warn(
            "Could not fetch the source for " +
              this.styleSheet.href +
              ", the editor was destroyed"
          );
          console.error(e);
        } else {
          console.error(e);
          this.emit("error", {
            key: LOAD_ERROR,
            append: this.styleSheet.href,
            level: "warning",
          });
          throw e;
        }
      });
  },

  /**
   * Set the cursor at the given line and column location within the code editor.
   *
   * @param {Number} line
   * @param {Number} column
   */
  setCursor(line, column) {
    line = line || 0;
    column = column || 0;

    const position = this.translateCursorPosition(line, column);
    this.sourceEditor.setCursor({ line: position.line, ch: position.column });
  },

  /**
   * If the stylesheet was automatically prettified, there should be a list of line
   * and column mappings from the original to the generated source that can be used
   * to translate the cursor position to the correct location in the prettified source.
   * If no mappings exist, return the original cursor position unchanged.
   *
   * @param  {Number} line
   * @param  {Numer} column
   *
   * @return {Object}
   */
  translateCursorPosition(line, column) {
    if (Array.isArray(this._mappings)) {
      for (const mapping of this._mappings) {
        if (
          mapping.original.line === line &&
          mapping.original.column === column
        ) {
          line = mapping.generated.line;
          column = mapping.generated.column;
          continue;
        }
      }
    }

    return { line, column };
  },

  /**
   * Forward property-change event from stylesheet.
   *
   * @param  {string} event
   *         Event type
   * @param  {string} property
   *         Property that has changed on sheet
   */
  onPropertyChange: function(property, value) {
    this.emit("property-change", property, value);
  },

  /**
   * Called when the stylesheet text changes.
   * @param {Object} update: The stylesheet resource update packet.
   */
  onStyleApplied: function(update) {
    const updateIsFromSyleSheetEditor =
      update?.event?.cause === STYLE_SHEET_UPDATE_CAUSED_BY_STYLE_EDITOR;

    if (updateIsFromSyleSheetEditor) {
      // We just applied an edit in the editor, so we can drop this notification.
      this.emit("style-applied");
      return;
    }

    if (this.sourceEditor) {
      this._getSourceTextAndPrettify().then(newText => {
        this._justSetText = true;
        const firstLine = this.sourceEditor.getFirstVisibleLine();
        const pos = this.sourceEditor.getCursor();
        this.sourceEditor.setText(newText);
        this.sourceEditor.setFirstVisibleLine(firstLine);
        this.sourceEditor.setCursor(pos);
        this.emit("style-applied");
      });
    }
  },

  /**
   * Handles changes to the list of @media rules in the stylesheet.
   * Emits 'media-rules-changed' if the list has changed.
   *
   * @param  {array} rules
   *         Array of MediaRuleFronts for new media rules of sheet.
   */
  onMediaRulesChanged: function(rules) {
    if (!rules.length && !this.mediaRules.length) {
      return;
    }

    this.mediaRules = rules;
    this.emitMediaRulesChanged();
  },

  /**
   * Forward media-rules-changed event from stylesheet.
   */
  emitMediaRulesChanged: function() {
    this.emit("media-rules-changed", this.mediaRules);
  },

  /**
   * Create source editor and load state into it.
   * @param  {DOMElement} inputElement
   *         Element to load source editor in
   * @param  {CssProperties} cssProperties
   *         A css properties database.
   *
   * @return {Promise}
   *         Promise that will resolve when the style editor is loaded.
   */
  load: async function(inputElement, cssProperties) {
    if (this._isDestroyed) {
      throw new Error(
        "Won't load source editor as the style sheet has " +
          "already been removed from Style Editor."
      );
    }

    this._inputElement = inputElement;

    const walker = await this.getWalker();
    const config = {
      value: this._state.text,
      lineNumbers: true,
      mode: Editor.modes.css,
      readOnly: false,
      autoCloseBrackets: "{}()",
      extraKeys: this._getKeyBindings(),
      contextMenu: "sourceEditorContextMenu",
      autocomplete: Services.prefs.getBoolPref(AUTOCOMPLETION_PREF),
      autocompleteOpts: { walker, cssProperties },
      cssProperties,
    };
    const sourceEditor = (this._sourceEditor = new Editor(config));

    sourceEditor.on("dirty-change", this.onPropertyChange);

    await sourceEditor.appendTo(inputElement);

    sourceEditor.on("saveRequested", this.saveToFile);

    if (!this.styleSheet.isOriginalSource) {
      sourceEditor.on("change", this.updateStyleSheet);
    }

    this.sourceEditor = sourceEditor;

    if (this._focusOnSourceEditorReady) {
      this._focusOnSourceEditorReady = false;
      sourceEditor.focus();
    }

    sourceEditor.setSelection(
      this._state.selection.start,
      this._state.selection.end
    );

    const highlighter = await this.getHighlighter();
    if (highlighter && walker && sourceEditor.container?.contentWindow) {
      sourceEditor.container.contentWindow.addEventListener(
        "mousemove",
        this._onMouseMove
      );
    }

    // Add the commands controller for the source-editor.
    sourceEditor.insertCommandsController();

    this.emit("source-editor-load");
  },

  /**
   * Get the source editor for this editor.
   *
   * @return {Promise}
   *         Promise that will resolve with the editor.
   */
  getSourceEditor: function() {
    const self = this;

    if (this.sourceEditor) {
      return Promise.resolve(this);
    }

    return new Promise(resolve => {
      this.on("source-editor-load", () => {
        resolve(self);
      });
    });
  },

  /**
   * Focus the Style Editor input.
   */
  focus: function() {
    if (this.sourceEditor) {
      this.sourceEditor.focus();
    } else {
      this._focusOnSourceEditorReady = true;
    }
  },

  /**
   * Event handler for when the editor is shown.
   *
   * @param {Object} options
   * @param {String} options.reason: Indicates why the editor is shown
   */
  onShow: function(options = {}) {
    if (this.sourceEditor) {
      // CodeMirror needs refresh to restore scroll position after hiding and
      // showing the editor.
      this.sourceEditor.refresh();
    }

    // We don't want to focus the editor if it was shown because of the list being filtered,
    // as the user might still be typing in the filter input.
    if (options.reason !== "filter-auto") {
      this.focus();
    }
  },

  /**
   * Toggled the disabled state of the underlying stylesheet.
   */
  async toggleDisabled() {
    const styleSheetsFront = await this._getStyleSheetsFront();
    styleSheetsFront.toggleDisabled(this.resourceId).catch(console.error);
  },

  /**
   * Queue a throttled task to update the live style sheet.
   */
  updateStyleSheet: function() {
    if (this._updateTask) {
      // cancel previous queued task not executed within throttle delay
      this._window.clearTimeout(this._updateTask);
    }

    this._updateTask = this._window.setTimeout(
      this._updateStyleSheet,
      UPDATE_STYLESHEET_DELAY
    );
  },

  /**
   * Update live style sheet according to modifications.
   */
  async _updateStyleSheet() {
    if (this.styleSheet.disabled) {
      // TODO: do we want to do this?
      return;
    }

    if (this._justSetText) {
      this._justSetText = false;
      return;
    }

    // reset only if we actually perform an update
    // (stylesheet is enabled) so that 'missed' updates
    // while the stylesheet is disabled can be performed
    // when it is enabled back. @see enableStylesheet
    this._updateTask = null;

    if (this.sourceEditor) {
      this._state.text = this.sourceEditor.getText();
    }

    try {
      const styleSheetsFront = await this._getStyleSheetsFront();
      await styleSheetsFront.update(
        this.resourceId,
        this._state.text,
        this.transitionsEnabled,
        STYLE_SHEET_UPDATE_CAUSED_BY_STYLE_EDITOR
      );

      // Clear any existing mappings from automatic CSS prettification
      // because they were likely invalided by manually editing the stylesheet.
      this._mappings = null;
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * Handle mousemove events, calling _highlightSelectorAt after a delay only
   * and reseting the delay everytime.
   */
  _onMouseMove: function(e) {
    // As we only want to hide an existing highlighter, we can use this.highlighter directly
    // (and not this.getHighlighter).
    if (this.highlighter) {
      this.highlighter.hide();
    }

    if (this.mouseMoveTimeout) {
      this._window.clearTimeout(this.mouseMoveTimeout);
      this.mouseMoveTimeout = null;
    }

    this.mouseMoveTimeout = this._window.setTimeout(() => {
      this._highlightSelectorAt(e.clientX, e.clientY);
    }, SELECTOR_HIGHLIGHT_TIMEOUT);
  },

  /**
   * Highlight nodes matching the selector found at coordinates x,y in the
   * editor, if any.
   *
   * @param {Number} x
   * @param {Number} y
   */
  async _highlightSelectorAt(x, y) {
    const pos = this.sourceEditor.getPositionFromCoords({ left: x, top: y });
    const info = this.sourceEditor.getInfoAt(pos);
    if (!info || info.state !== "selector") {
      return;
    }

    const onGetHighlighter = this.getHighlighter();
    const walker = await this.getWalker();
    const node = await walker.getStyleSheetOwnerNode(this.resourceId);

    const highlighter = await onGetHighlighter;
    await highlighter.show(node, {
      selector: info.selector,
      hideInfoBar: true,
      showOnly: "border",
      region: "border",
    });

    this.emit("node-highlighted");
  },

  /**
   * Returns the walker front associated with this._resource target.
   *
   * @returns {Promise<WalkerFront>}
   */
  async getWalker() {
    if (this.walker) {
      return this.walker;
    }

    const { targetFront } = this._resource;
    const inspectorFront = await targetFront.getFront("inspector");
    this.walker = inspectorFront.walker;
    return this.walker;
  },

  /**
   * Returns or creates the selector highlighter associated with this._resource target.
   *
   * @returns {CustomHighlighterFront|null}
   */
  async getHighlighter() {
    if (this.highlighter) {
      return this.highlighter;
    }

    const walker = await this.getWalker();
    try {
      this.highlighter = await walker.parentFront.getHighlighterByType(
        SELECTOR_HIGHLIGHTER_TYPE
      );
      return this.highlighter;
    } catch (e) {
      // The selectorHighlighter can't always be instantiated, for example
      // it doesn't work with XUL windows (until bug 1094959 gets fixed);
      // or the selectorHighlighter doesn't exist on the backend.
      console.warn(
        "The selectorHighlighter couldn't be instantiated, " +
          "elements matching hovered selectors will not be highlighted"
      );
    }
    return null;
  },

  /**
   * Save the editor contents into a file and set savedFile property.
   * A file picker UI will open if file is not set and editor is not headless.
   *
   * @param mixed file
   *        Optional nsIFile or string representing the filename to save in the
   *        background, no UI will be displayed.
   *        If not specified, the original style sheet URI is used.
   *        To implement 'Save' instead of 'Save as', you can pass
   *        savedFile here.
   * @param function(nsIFile aFile) callback
   *        Optional callback called when the operation has finished.
   *        aFile has the nsIFile object for saved file or null if the operation
   *        has failed or has been canceled by the user.
   * @see savedFile
   */
  saveToFile: function(file, callback) {
    const onFile = returnFile => {
      if (!returnFile) {
        if (callback) {
          callback(null);
        }
        return;
      }

      if (this.sourceEditor) {
        this._state.text = this.sourceEditor.getText();
      }

      const ostream = lazy.FileUtils.openSafeFileOutputStream(returnFile);
      const buffer = new TextEncoder().encode(this._state.text).buffer;
      const istream = new lazy.BufferStream(buffer, 0, buffer.byteLength);

      lazy.NetUtil.asyncCopy(istream, ostream, status => {
        if (!Components.isSuccessCode(status)) {
          if (callback) {
            callback(null);
          }
          this.emit("error", { key: SAVE_ERROR });
          return;
        }
        lazy.FileUtils.closeSafeFileOutputStream(ostream);

        this.onFileSaved(returnFile);

        if (callback) {
          callback(returnFile);
        }
      });
    };

    let defaultName;
    if (this._friendlyName) {
      defaultName = PathUtils.isAbsolute(this._friendlyName)
        ? PathUtils.filename(this._friendlyName)
        : this._friendlyName;
    }
    showFilePicker(
      file || this._styleSheetFilePath,
      true,
      this._window,
      onFile,
      defaultName
    );
  },

  /**
   * Called when this source has been successfully saved to disk.
   */
  onFileSaved: function(returnFile) {
    this._friendlyName = null;
    this.savedFile = returnFile;

    if (this.sourceEditor) {
      this.sourceEditor.setClean();
    }

    this.emit("property-change");

    // TODO: replace with file watching
    this._modCheckCount = 0;
    this._window.clearTimeout(this._timeout);

    if (this.linkedCSSFile && !this.linkedCSSFileError) {
      this._timeout = this._window.setTimeout(
        this.checkLinkedFileForChanges,
        CHECK_LINKED_SHEET_DELAY
      );
    }
  },

  /**
   * Check to see if our linked CSS file has changed on disk, and
   * if so, update the live style sheet.
   */
  checkLinkedFileForChanges: function() {
    IOUtils.stat(this.linkedCSSFile).then(info => {
      const lastChange = info.lastModified;

      if (this._fileModDate && lastChange != this._fileModDate) {
        this._fileModDate = lastChange;
        this._modCheckCount = 0;

        this.updateLinkedStyleSheet();
        return;
      }

      if (++this._modCheckCount > MAX_CHECK_COUNT) {
        this.updateLinkedStyleSheet();
        return;
      }

      // try again in a bit
      this._timeout = this._window.setTimeout(
        this.checkLinkedFileForChanges,
        CHECK_LINKED_SHEET_DELAY
      );
    }, this.markLinkedFileBroken);
  },

  /**
   * Notify that the linked CSS file (if this is an original source)
   * doesn't exist on disk in the place we think it does.
   *
   * @param string error
   *        The error we got when trying to access the file.
   */
  markLinkedFileBroken: function(error) {
    this.linkedCSSFileError = error || true;
    this.emit("linked-css-file-error");

    error +=
      " querying " +
      this.linkedCSSFile +
      " original source location: " +
      this.savedFile.path;
    console.error(error);
  },

  /**
   * For original sources (e.g. Sass files). Fetch contents of linked CSS
   * file from disk and live update the stylesheet object with the contents.
   */
  updateLinkedStyleSheet: function() {
    IOUtils.read(this.linkedCSSFile).then(async array => {
      const decoder = new TextDecoder();
      const text = decoder.decode(array);

      // Ensure we don't re-fetch the text from the original source
      // actor when we're notified that the style sheet changed.
      const styleSheetsFront = await this._getStyleSheetsFront();

      await styleSheetsFront.update(
        this.resourceId,
        text,
        this.transitionsEnabled,
        STYLE_SHEET_UPDATE_CAUSED_BY_STYLE_EDITOR
      );
    }, this.markLinkedFileBroken);
  },

  /**
   * Retrieve custom key bindings objects as expected by Editor.
   * Editor action names are not displayed to the user.
   *
   * @return {array} key binding objects for the source editor
   */
  _getKeyBindings: function() {
    const saveStyleSheetKeybind = Editor.accel(
      getString("saveStyleSheet.commandkey")
    );
    const focusFilterInputKeybind = Editor.accel(
      getString("focusFilterInput.commandkey")
    );

    return {
      Esc: false,
      [saveStyleSheetKeybind]: () => {
        this.saveToFile(this.savedFile);
      },
      ["Shift-" + saveStyleSheetKeybind]: () => {
        this.saveToFile();
      },
      // We can't simply ignore this (with `false`, or returning `CodeMirror.Pass`), as the
      // event isn't received by the event listener in StyleSheetUI.
      [focusFilterInputKeybind]: () => {
        this.emit("filter-input-keyboard-shortcut");
      },
    };
  },

  _getStyleSheetsFront() {
    return this._resource.targetFront.getFront("stylesheets");
  },

  /**
   * Clean up for this editor.
   */
  destroy: function() {
    if (this._sourceEditor) {
      this._sourceEditor.off("dirty-change", this.onPropertyChange);
      this._sourceEditor.off("saveRequested", this.saveToFile);
      this._sourceEditor.off("change", this.updateStyleSheet);
      if (this._sourceEditor.container?.contentWindow) {
        this._sourceEditor.container.contentWindow.removeEventListener(
          "mousemove",
          this._onMouseMove
        );
      }
      this._sourceEditor.destroy();
    }
    this._isDestroyed = true;
  },
};

/**
 * Find a path on disk for a file given it's hosted uri, the uri of the
 * original resource that generated it (e.g. Sass file), and the location of the
 * local file for that source.
 *
 * @param {nsIURI} uri
 *        The uri of the resource
 * @param {nsIURI} origUri
 *        The uri of the original source for the resource
 * @param {nsIFile} file
 *        The local file for the resource on disk
 *
 * @return {string}
 *         The path of original file on disk
 */
function findLinkedFilePath(uri, origUri, file) {
  const { origBranch, branch } = findUnsharedBranches(origUri, uri);
  const project = findProjectPath(file, origBranch);

  const parts = project.concat(branch);
  const path = PathUtils.join.apply(this, parts);

  return path;
}

/**
 * Find the path of a project given a file in the project and its branch
 * off the root. e.g.:
 * /Users/moz/proj/src/a.css" and "src/a.css"
 * would yield ["Users", "moz", "proj"]
 *
 * @param {nsIFile} file
 *        file for that resource on disk
 * @param {array} branch
 *        path parts for branch to chop off file path.
 * @return {array}
 *        array of path parts
 */
function findProjectPath(file, branch) {
  const path = PathUtils.split(file.path);

  for (let i = 2; i <= branch.length; i++) {
    // work backwards until we find a differing directory name
    if (path[path.length - i] != branch[branch.length - i]) {
      return path.slice(0, path.length - i + 1);
    }
  }

  // if we don't find a differing directory, just chop off the branch
  return path.slice(0, path.length - branch.length);
}

/**
 * Find the parts of a uri past the root it shares with another uri. e.g:
 * "http://localhost/built/a.scss" and "http://localhost/src/a.css"
 * would yield ["built", "a.scss"] and ["src", "a.css"]
 *
 * @param {nsIURI} origUri
 *        uri to find unshared branch of. Usually is uri for original source.
 * @param {nsIURI} uri
 *        uri to compare against to get a shared root
 * @return {object}
 *         object with 'branch' and 'origBranch' array of path parts for branch
 */
function findUnsharedBranches(origUri, uri) {
  origUri = PathUtils.split(origUri.pathQueryRef);
  uri = PathUtils.split(uri.pathQueryRef);

  for (let i = 0; i < uri.length - 1; i++) {
    if (uri[i] != origUri[i]) {
      return {
        branch: uri.slice(i),
        origBranch: origUri.slice(i),
      };
    }
  }
  return {
    branch: uri,
    origBranch: origUri,
  };
}

/**
 * Remove the query string from a url.
 *
 * @param  {string} href
 *         Url to remove query string from
 * @return {string}
 *         Url without query string
 */
function removeQuery(href) {
  return href.replace(/\?.*/, "");
}
