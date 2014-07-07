/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleSheetEditor", "prettifyCSS"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
const Editor  = require("devtools/sourceeditor/editor");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const {CssLogic} = require("devtools/styleinspector/css-logic");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/devtools/event-emitter.js");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");

const LOAD_ERROR = "error-load";
const SAVE_ERROR = "error-save";

// max update frequency in ms (avoid potential typing lag and/or flicker)
// @see StyleEditor.updateStylesheet
const UPDATE_STYLESHEET_THROTTLE_DELAY = 500;

// Pref which decides if CSS autocompletion is enabled in Style Editor or not.
const AUTOCOMPLETION_PREF = "devtools.styleeditor.autocompletion-enabled";

// How long to wait to update linked CSS file after original source was saved
// to disk. Time in ms.
const CHECK_LINKED_SHEET_DELAY=500;

// How many times to check for linked file changes
const MAX_CHECK_COUNT=10;

// The classname used to show a line that is not used
const UNUSED_CLASS = "cm-unused-line";

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
 */
function StyleSheetEditor(styleSheet, win, file, isNew, walker) {
  EventEmitter.decorate(this);

  this.styleSheet = styleSheet;
  this._inputElement = null;
  this.sourceEditor = null;
  this._window = win;
  this._isNew = isNew;
  this.walker = walker;

  this._state = {   // state to use when inputElement attaches
    text: "",
    selection: {
      start: {line: 0, ch: 0},
      end: {line: 0, ch: 0}
    },
    topIndex: 0              // the first visible line
  };

  this._styleSheetFilePath = null;
  if (styleSheet.href &&
      Services.io.extractScheme(this.styleSheet.href) == "file") {
    this._styleSheetFilePath = this.styleSheet.href;
  }

  this._onPropertyChange = this._onPropertyChange.bind(this);
  this._onError = this._onError.bind(this);
  this._onMediaRuleMatchesChange = this._onMediaRuleMatchesChange.bind(this);
  this._onMediaRulesChanged = this._onMediaRulesChanged.bind(this)
  this.checkLinkedFileForChanges = this.checkLinkedFileForChanges.bind(this);
  this.markLinkedFileBroken = this.markLinkedFileBroken.bind(this);
  this.saveToFile = this.saveToFile.bind(this);
  this.updateStyleSheet = this.updateStyleSheet.bind(this);

  this._focusOnSourceEditorReady = false;
  this.cssSheet.on("property-change", this._onPropertyChange);
  this.styleSheet.on("error", this._onError);
  this.mediaRules = [];
  if (this.cssSheet.getMediaRules) {
    this.cssSheet.getMediaRules().then(this._onMediaRulesChanged);
  }
  this.cssSheet.on("media-rules-changed", this._onMediaRulesChanged);
  this.savedFile = file;
  this.linkCSSFile();
}

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
      let index = this.styleSheet.styleSheetIndex + 1;
      return _("newStyleSheet", index);
    }

    if (!this.styleSheet.href) {
      let index = this.styleSheet.styleSheetIndex + 1;
      return _("inlineStyleSheet", index);
    }

    if (!this._friendlyName) {
      let sheetURI = this.styleSheet.href;
      this._friendlyName = CssLogic.shortSource({ href: sheetURI });
      try {
        this._friendlyName = decodeURI(this._friendlyName);
      } catch (ex) {
      }
    }
    return this._friendlyName;
  },

  /**
   * If this is an original source, get the path of the CSS file it generated.
   */
  linkCSSFile: function() {
    if (!this.styleSheet.isOriginalSource) {
      return;
    }

    let relatedSheet = this.styleSheet.relatedStyleSheet;

    let path;
    let href = removeQuery(relatedSheet.href);
    let uri = NetUtil.newURI(href);

    if (uri.scheme == "file") {
      let file = uri.QueryInterface(Ci.nsIFileURL).file;
      path = file.path;
    }
    else if (this.savedFile) {
      let origHref = removeQuery(this.styleSheet.href);
      let origUri = NetUtil.newURI(origHref);
      path = findLinkedFilePath(uri, origUri, this.savedFile);
    }
    else {
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
   * Start fetching the full text source for this editor's sheet.
   */
  fetchSource: function(callback) {
    return this.styleSheet.getText().then((longStr) => {
      longStr.string().then((source) => {
        let ruleCount = this.styleSheet.ruleCount;
        this._state.text = CssLogic.prettifyCSS(source, ruleCount);
        this.sourceLoaded = true;

        if (callback) {
          callback(source);
        }
        return source;
      });
    }, e => {
      this.emit("error", { key: LOAD_ERROR, append: this.styleSheet.href });
      throw e;
    })
  },

  /**
   * Add markup to a region. UNUSED_CLASS is added to specified lines
   * @param region An object shaped like
   *   {
   *     start: { line: L1, column: C1 },
   *     end: { line: L2, column: C2 }    // optional
   *   }
   */
  addUnusedRegion: function(region) {
    this.sourceEditor.addLineClass(region.start.line - 1, UNUSED_CLASS);
    if (region.end) {
      for (let i = region.start.line; i <= region.end.line; i++) {
        this.sourceEditor.addLineClass(i - 1, UNUSED_CLASS);
      }
    }
  },

  /**
   * As addUnusedRegion except that it takes an array of regions
   */
  addUnusedRegions: function(regions) {
    for (let region of regions) {
      this.addUnusedRegion(region);
    }
  },

  /**
   * Remove all the unused markup regions added by addUnusedRegion
   */
  removeAllUnusedRegions: function() {
    for (let i = 0; i < this.sourceEditor.lineCount(); i++) {
      this.sourceEditor.removeLineClass(i, UNUSED_CLASS);
    }
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
    for (let rule of this.mediaRules) {
      rule.off("matches-change", this._onMediaRuleMatchesChange);
      rule.destroy();
    }
    this.mediaRules = rules;

    for (let rule of rules) {
      rule.on("matches-change", this._onMediaRuleMatchesChange);
    }
    this.emit("media-rules-changed", rules);
  },

  /**
   * Forward media-rules-changed event from stylesheet.
   */
  _onMediaRuleMatchesChange: function() {
    this.emit("media-rules-changed", this.mediaRules);
  },

  /**
   * Forward error event from stylesheet.
   *
   * @param  {string} event
   *         Event type
   * @param  {string} errorCode
   */
  _onError: function(event, data) {
    this.emit("error", data);
  },

  /**
   * Create source editor and load state into it.
   * @param  {DOMElement} inputElement
   *         Element to load source editor in
   *
   * @return {Promise}
   *         Promise that will resolve when the style editor is loaded.
   */
  load: function(inputElement) {
    this._inputElement = inputElement;

    let config = {
      value: this._state.text,
      lineNumbers: true,
      mode: Editor.modes.css,
      readOnly: false,
      autoCloseBrackets: "{}()[]",
      extraKeys: this._getKeyBindings(),
      contextMenu: "sourceEditorContextMenu",
      autocomplete: Services.prefs.getBoolPref(AUTOCOMPLETION_PREF),
      autocompleteOpts: { walker: this.walker }
    };
    let sourceEditor = this._sourceEditor = new Editor(config);

    sourceEditor.on("dirty-change", this._onPropertyChange);

    return sourceEditor.appendTo(inputElement).then(() => {
      sourceEditor.on("save", this.saveToFile);

      if (this.styleSheet.update) {
        sourceEditor.on("change", this.updateStyleSheet);
      }

      this.sourceEditor = sourceEditor;

      if (this._focusOnSourceEditorReady) {
        this._focusOnSourceEditorReady = false;
        sourceEditor.focus();
      }

      sourceEditor.setFirstVisibleLine(this._state.topIndex);
      sourceEditor.setSelection(this._state.selection.start,
                                this._state.selection.end);

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
    let deferred = promise.defer();

    if (this.sourceEditor) {
      return promise.resolve(this);
    }
    this.on("source-editor-load", () => {
      deferred.resolve(this);
    });
    return deferred.promise;
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
      this.sourceEditor.setFirstVisibleLine(this._state.topIndex);
    }
    this.focus();
  },

  /**
   * Toggled the disabled state of the underlying stylesheet.
   */
  toggleDisabled: function() {
    this.styleSheet.toggleDisabled();
  },

  /**
   * Queue a throttled task to update the live style sheet.
   *
   * @param boolean immediate
   *        Optional. If true the update is performed immediately.
   */
  updateStyleSheet: function(immediate) {
    if (this._updateTask) {
      // cancel previous queued task not executed within throttle delay
      this._window.clearTimeout(this._updateTask);
    }

    if (immediate) {
      this._updateStyleSheet();
    } else {
      this._updateTask = this._window.setTimeout(this._updateStyleSheet.bind(this),
                                           UPDATE_STYLESHEET_THROTTLE_DELAY);
    }
  },

  /**
   * Update live style sheet according to modifications.
   */
  _updateStyleSheet: function() {
    if (this.styleSheet.disabled) {
      return;  // TODO: do we want to do this?
    }

    this._updateTask = null; // reset only if we actually perform an update
                             // (stylesheet is enabled) so that 'missed' updates
                             // while the stylesheet is disabled can be performed
                             // when it is enabled back. @see enableStylesheet

    if (this.sourceEditor) {
      this._state.text = this.sourceEditor.getText();
    }

    this.styleSheet.update(this._state.text, true);
  },

  /**
   * Save the editor contents into a file and set savedFile property.
   * A file picker UI will open if file is not set and editor is not headless.
   *
   * @param mixed file
   *        Optional nsIFile or string representing the filename to save in the
   *        background, no UI will be displayed.
   *        If not specified, the original style sheet URI is used.
   *        To implement 'Save' instead of 'Save as', you can pass savedFile here.
   * @param function(nsIFile aFile) callback
   *        Optional callback called when the operation has finished.
   *        aFile has the nsIFile object for saved file or null if the operation
   *        has failed or has been canceled by the user.
   * @see savedFile
   */
  saveToFile: function(file, callback) {
    let onFile = (returnFile) => {
      if (!returnFile) {
        if (callback) {
          callback(null);
        }
        return;
      }

      if (this.sourceEditor) {
        this._state.text = this.sourceEditor.getText();
      }

      let ostream = FileUtils.openSafeFileOutputStream(returnFile);
      let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                        .createInstance(Ci.nsIScriptableUnicodeConverter);
      converter.charset = "UTF-8";
      let istream = converter.convertToInputStream(this._state.text);

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
      let lastChange = info.lastModificationDate.getTime();

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
             " original source location: " + this.savedFile.path
    Cu.reportError(error);
  },

  /**
   * For original sources (e.g. Sass files). Fetch contents of linked CSS
   * file from disk and live update the stylesheet object with the contents.
   */
  updateLinkedStyleSheet: function() {
    OS.File.read(this.linkedCSSFile).then((array) => {
      let decoder = new TextDecoder();
      let text = decoder.decode(array);

      let relatedSheet = this.styleSheet.relatedStyleSheet;
      relatedSheet.update(text, true);
    }, this.markLinkedFileBroken);
  },

  /**
    * Retrieve custom key bindings objects as expected by Editor.
    * Editor action names are not displayed to the user.
    *
    * @return {array} key binding objects for the source editor
    */
  _getKeyBindings: function() {
    let bindings = {};

    bindings[Editor.accel(_("saveStyleSheet.commandkey"))] = () => {
      this.saveToFile(this.savedFile);
    };

    bindings["Shift-" + Editor.accel(_("saveStyleSheet.commandkey"))] = () => {
      this.saveToFile();
    };

    return bindings;
  },

  /**
   * Clean up for this editor.
   */
  destroy: function() {
    if (this._sourceEditor) {
      this._sourceEditor.off("dirty-change", this._onPropertyChange);
      this._sourceEditor.off("save", this.saveToFile);
      this._sourceEditor.off("change", this.updateStyleSheet);
      this._sourceEditor.destroy();
    }
    this.cssSheet.off("property-change", this._onPropertyChange);
    this.cssSheet.off("media-rules-changed", this._onMediaRulesChanged);
    this.styleSheet.off("error", this._onError);
  }
}

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
  let { origBranch, branch } = findUnsharedBranches(origUri, uri);
  let project = findProjectPath(file, origBranch);

  let parts = project.concat(branch);
  let path = OS.Path.join.apply(this, parts);

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
  let path = OS.Path.split(file.path).components;

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
  origUri = OS.Path.split(origUri.path).components;
  uri = OS.Path.split(uri.path).components;

  for (let i = 0; i < uri.length - 1; i++) {
    if (uri[i] != origUri[i]) {
      return {
        branch: uri.slice(i),
        origBranch: origUri.slice(i)
      };
    }
  }
  return {
    branch: uri,
    origBranch: origUri
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
