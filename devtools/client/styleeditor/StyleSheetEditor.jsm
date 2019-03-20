/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleSheetEditor"];

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const Editor = require("devtools/client/shared/sourceeditor/editor");
const promise = require("promise");
const {shortSource, prettifyCSS} = require("devtools/shared/inspector/css-logic");
const {throttle} = require("devtools/shared/throttle");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");
const {FileUtils} = require("resource://gre/modules/FileUtils.jsm");
const {NetUtil} = require("resource://gre/modules/NetUtil.jsm");
const {OS} = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const {
  getString,
  showFilePicker,
} = require("resource://devtools/client/styleeditor/StyleEditorUtil.jsm");

const LOAD_ERROR = "error-load";
const SAVE_ERROR = "error-save";

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

/**
 * StyleSheetEditor controls the editor linked to a particular StyleSheet
 * object.
 *
 * Emits events:
 *   'property-change': A property on the underlying stylesheet has changed
 *   'source-editor-load': The source editor for this editor has been loaded
 *   'error': An error has occured
 *
 * @param {StyleSheet|OriginalSource}  styleSheet
 *        Stylesheet or original source to show
 * @param {DOMWindow}  win
 *        panel window for style editor
 * @param {nsIFile}  file
 *        Optional file that the sheet was imported from
 * @param {boolean} isNew
 *        Optional whether the sheet was created by the user
 * @param {Walker} walker
 *        Optional walker used for selectors autocompletion
 * @param {CustomHighlighterFront} highlighter
 *        Optional highlighter front for the SelectorHighligher used to
 *        highlight selectors
 */
function StyleSheetEditor(styleSheet, win, file, isNew, walker, highlighter) {
  EventEmitter.decorate(this);

  this.styleSheet = styleSheet;
  this._inputElement = null;
  this.sourceEditor = null;
  this._window = win;
  this._isNew = isNew;
  this.walker = walker;
  this.highlighter = highlighter;

  // True when we've called update() on the style sheet.
  this._isUpdating = false;
  // True when we've just set the editor text based on a style-applied
  // event from the StyleSheetActor.
  this._justSetText = false;

  // state to use when inputElement attaches
  this._state = {
    text: "",
    selection: {
      start: {line: 0, ch: 0},
      end: {line: 0, ch: 0},
    },
  };

  this._styleSheetFilePath = null;
  if (styleSheet.href &&
      Services.io.extractScheme(this.styleSheet.href) == "file") {
    this._styleSheetFilePath = this.styleSheet.href;
  }

  this._onPropertyChange = this._onPropertyChange.bind(this);
  this._onError = this._onError.bind(this);
  this._onMediaRulesChanged = this._onMediaRulesChanged.bind(this);
  this._onStyleApplied = this._onStyleApplied.bind(this);
  this.checkLinkedFileForChanges = this.checkLinkedFileForChanges.bind(this);
  this.markLinkedFileBroken = this.markLinkedFileBroken.bind(this);
  this.saveToFile = this.saveToFile.bind(this);
  this.updateStyleSheet = this.updateStyleSheet.bind(this);
  this._updateStyleSheet = this._updateStyleSheet.bind(this);
  this._onMouseMove = this._onMouseMove.bind(this);

  this.emitMediaRulesChanged =
    throttle(this.emitMediaRulesChanged, EMIT_MEDIA_RULES_THROTTLING, this);

  this._focusOnSourceEditorReady = false;
  this.cssSheet.on("property-change", this._onPropertyChange);
  this.styleSheet.on("error", this._onError);
  this.mediaRules = [];
  if (this.cssSheet.getMediaRules) {
    this.cssSheet.getMediaRules().then(this._onMediaRulesChanged,
                                       console.error);
  }
  this.cssSheet.on("media-rules-changed", this._onMediaRulesChanged);
  this.cssSheet.on("style-applied", this._onStyleApplied);
  this.savedFile = file;
  this.linkCSSFile();
}
this.StyleSheetEditor = StyleSheetEditor;

StyleSheetEditor.prototype = {
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
      const index = this.styleSheet.styleSheetIndex + 1;
      return getString("newStyleSheet", index);
    }

    if (!this.styleSheet.href) {
      const index = this.styleSheet.styleSheetIndex + 1;
      return getString("inlineStyleSheet", index);
    }

    if (!this._friendlyName) {
      const sheetURI = this.styleSheet.href;
      this._friendlyName = shortSource({ href: sheetURI });
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
    const uri = NetUtil.newURI(href);

    if (uri.scheme == "file") {
      const file = uri.QueryInterface(Ci.nsIFileURL).file;
      path = file.path;
    } else if (this.savedFile) {
      const origHref = removeQuery(this.styleSheet.href);
      const origUri = NetUtil.newURI(origHref);
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
    OS.File.stat(path).then((info) => {
      this._fileModDate = info.lastModificationDate.getTime();
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
  _getSourceTextAndPrettify: function() {
    return this.styleSheet.getText().then((longStr) => {
      return longStr.string();
    }).then((source) => {
      const ruleCount = this.styleSheet.ruleCount;
      if (!this.styleSheet.isOriginalSource) {
        source = prettifyCSS(source, ruleCount);
      }
      this._state.text = source;
      return source;
    });
  },

  /**
   * Start fetching the full text source for this editor's sheet.
   *
   * @return {Promise}
   *         A promise that'll resolve with the source text once the source
   *         has been loaded or reject on unexpected error.
   */
  fetchSource: function() {
    return this._getSourceTextAndPrettify().then((source) => {
      this.sourceLoaded = true;
      return source;
    }).catch(e => {
      if (this._isDestroyed) {
        console.warn("Could not fetch the source for " +
                     this.styleSheet.href +
                     ", the editor was destroyed");
        console.error(e);
      } else {
        console.error(e);
        this.emit("error", { key: LOAD_ERROR, append: this.styleSheet.href,
                             level: "warning" });
        throw e;
      }
    });
  },

  /**
   * Forward property-change event from stylesheet.
   *
   * @param  {string} event
   *         Event type
   * @param  {string} property
   *         Property that has changed on sheet
   */
  _onPropertyChange: function(property, value) {
    this.emit("property-change", property, value);
  },

  /**
   * Called when the stylesheet text changes.
   */
  _onStyleApplied: function() {
    if (this._isUpdating) {
      // We just applied an edit in the editor, so we can drop this
      // notification.
      this._isUpdating = false;
      this.emit("style-applied");
    } else if (this.sourceEditor) {
      this._getSourceTextAndPrettify().then((newText) => {
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
  _onMediaRulesChanged: function(rules) {
    if (!rules.length && !this.mediaRules.length) {
      return;
    }
    for (const rule of this.mediaRules) {
      rule.off("matches-change", this.emitMediaRulesChanged);
      rule.destroy();
    }
    this.mediaRules = rules;

    for (const rule of rules) {
      rule.on("matches-change", this.emitMediaRulesChanged);
    }
    this.emitMediaRulesChanged();
  },

  /**
   * Forward media-rules-changed event from stylesheet.
   */
  emitMediaRulesChanged: function() {
    this.emit("media-rules-changed", this.mediaRules);
  },

  /**
   * Forward error event from stylesheet.
   *
   * @param  {Object} data: The parameters to customize the error message
   */
  _onError: function(data) {
    this.emit("error", data);
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
  load: function(inputElement, cssProperties) {
    if (this._isDestroyed) {
      return promise.reject("Won't load source editor as the style sheet has " +
                            "already been removed from Style Editor.");
    }

    this._inputElement = inputElement;

    const config = {
      value: this._state.text,
      lineNumbers: true,
      mode: Editor.modes.css,
      readOnly: false,
      autoCloseBrackets: "{}()",
      extraKeys: this._getKeyBindings(),
      contextMenu: "sourceEditorContextMenu",
      autocomplete: Services.prefs.getBoolPref(AUTOCOMPLETION_PREF),
      autocompleteOpts: { walker: this.walker, cssProperties },
      cssProperties,
    };
    const sourceEditor = this._sourceEditor = new Editor(config);

    sourceEditor.on("dirty-change", this._onPropertyChange);

    return sourceEditor.appendTo(inputElement).then(() => {
      sourceEditor.on("saveRequested", this.saveToFile);

      if (this.styleSheet.update) {
        sourceEditor.on("change", this.updateStyleSheet);
      }

      this.sourceEditor = sourceEditor;

      if (this._focusOnSourceEditorReady) {
        this._focusOnSourceEditorReady = false;
        sourceEditor.focus();
      }

      sourceEditor.setSelection(this._state.selection.start,
                                this._state.selection.end);

      if (this.highlighter && this.walker) {
        sourceEditor.container.addEventListener("mousemove", this._onMouseMove);
      }

      // Add the commands controller for the source-editor.
      sourceEditor.insertCommandsController();

      this.emit("source-editor-load");
    });
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
   */
  onShow: function() {
    if (this.sourceEditor) {
      // CodeMirror needs refresh to restore scroll position after hiding and
      // showing the editor.
      this.sourceEditor.refresh();
    }
    this.focus();
  },

  /**
   * Toggled the disabled state of the underlying stylesheet.
   */
  toggleDisabled: function() {
    this.styleSheet.toggleDisabled().catch(console.error);
  },

  /**
   * Queue a throttled task to update the live style sheet.
   */
  updateStyleSheet: function() {
    if (this._updateTask) {
      // cancel previous queued task not executed within throttle delay
      this._window.clearTimeout(this._updateTask);
    }

    this._updateTask = this._window.setTimeout(this._updateStyleSheet,
                                               UPDATE_STYLESHEET_DELAY);
  },

  /**
   * Update live style sheet according to modifications.
   */
  _updateStyleSheet: function() {
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

    this._isUpdating = true;
    this.styleSheet.update(this._state.text, this.transitionsEnabled)
      .catch(console.error);
  },

  /**
   * Handle mousemove events, calling _highlightSelectorAt after a delay only
   * and reseting the delay everytime.
   */
  _onMouseMove: function(e) {
    this.highlighter.hide();

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
    const pos = this.sourceEditor.getPositionFromCoords({left: x, top: y});
    const info = this.sourceEditor.getInfoAt(pos);
    if (!info || info.state !== "selector") {
      return;
    }

    const node =
        await this.walker.getStyleSheetOwnerNode(this.styleSheet.actorID);
    await this.highlighter.show(node, {
      selector: info.selector,
      hideInfoBar: true,
      showOnly: "border",
      region: "border",
    });

    this.emit("node-highlighted");
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
    const onFile = (returnFile) => {
      if (!returnFile) {
        if (callback) {
          callback(null);
        }
        return;
      }

      if (this.sourceEditor) {
        this._state.text = this.sourceEditor.getText();
      }

      const ostream = FileUtils.openSafeFileOutputStream(returnFile);
      const converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                        .createInstance(Ci.nsIScriptableUnicodeConverter);
      converter.charset = "UTF-8";
      const istream = converter.convertToInputStream(this._state.text);

      NetUtil.asyncCopy(istream, ostream, (status) => {
        if (!Components.isSuccessCode(status)) {
          if (callback) {
            callback(null);
          }
          this.emit("error", { key: SAVE_ERROR });
          return;
        }
        FileUtils.closeSafeFileOutputStream(ostream);

        this.onFileSaved(returnFile);

        if (callback) {
          callback(returnFile);
        }
      });
    };

    let defaultName;
    if (this._friendlyName) {
      defaultName = OS.Path.basename(this._friendlyName);
    }
    showFilePicker(file || this._styleSheetFilePath, true, this._window,
                   onFile, defaultName);
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
      this._timeout = this._window.setTimeout(this.checkLinkedFileForChanges,
                                              CHECK_LINKED_SHEET_DELAY);
    }
  },

  /**
   * Check to see if our linked CSS file has changed on disk, and
   * if so, update the live style sheet.
   */
  checkLinkedFileForChanges: function() {
    OS.File.stat(this.linkedCSSFile).then((info) => {
      const lastChange = info.lastModificationDate.getTime();

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
      this._timeout = this._window.setTimeout(this.checkLinkedFileForChanges,
                                              CHECK_LINKED_SHEET_DELAY);
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

    error += " querying " + this.linkedCSSFile +
             " original source location: " + this.savedFile.path;
    console.error(error);
  },

  /**
   * For original sources (e.g. Sass files). Fetch contents of linked CSS
   * file from disk and live update the stylesheet object with the contents.
   */
  updateLinkedStyleSheet: function() {
    OS.File.read(this.linkedCSSFile).then((array) => {
      const decoder = new TextDecoder();
      const text = decoder.decode(array);

      // Ensure we don't re-fetch the text from the original source
      // actor when we're notified that the style sheet changed.
      this._isUpdating = true;
      const relatedSheet = this.styleSheet.relatedStyleSheet;
      relatedSheet.update(text, this.transitionsEnabled);
    }, this.markLinkedFileBroken);
  },

  /**
    * Retrieve custom key bindings objects as expected by Editor.
    * Editor action names are not displayed to the user.
    *
    * @return {array} key binding objects for the source editor
    */
  _getKeyBindings: function() {
    const bindings = {};
    const keybind = Editor.accel(getString("saveStyleSheet.commandkey"));

    bindings[keybind] = () => {
      this.saveToFile(this.savedFile);
    };

    bindings["Shift-" + keybind] = () => {
      this.saveToFile();
    };

    bindings.Esc = false;

    return bindings;
  },

  /**
   * Clean up for this editor.
   */
  destroy: function() {
    if (this._sourceEditor) {
      this._sourceEditor.off("dirty-change", this._onPropertyChange);
      this._sourceEditor.off("saveRequested", this.saveToFile);
      this._sourceEditor.off("change", this.updateStyleSheet);
      if (this.highlighter && this.walker && this._sourceEditor.container) {
        this._sourceEditor.container.removeEventListener("mousemove",
          this._onMouseMove);
      }
      this._sourceEditor.destroy();
    }
    this.cssSheet.off("property-change", this._onPropertyChange);
    this.cssSheet.off("media-rules-changed", this._onMediaRulesChanged);
    this.cssSheet.off("style-applied", this._onStyleApplied);
    this.styleSheet.off("error", this._onError);
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
  const path = OS.Path.join.apply(this, parts);

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
  const path = OS.Path.split(file.path).components;

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
  origUri = OS.Path.split(origUri.pathQueryRef).components;
  uri = OS.Path.split(uri.pathQueryRef).components;

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
