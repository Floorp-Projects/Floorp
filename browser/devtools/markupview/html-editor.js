/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 "use strict";

const {Cu} = require("chrome");
const Editor = require("devtools/sourceeditor/editor");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");

exports.HTMLEditor = HTMLEditor;

function ctrl(k) {
  return (Services.appinfo.OS == "Darwin" ? "Cmd-" : "Ctrl-") + k;
}
function stopPropagation(e) {
  e.stopPropagation();
}
/**
 * A wrapper around the Editor component, that allows editing of HTML.
 *
 * The main functionality this provides around the Editor is the ability
 * to show/hide/position an editor inplace. It only appends once to the
 * body, and uses CSS to position the editor.  The reason it is done this
 * way is that the editor is loaded in an iframe, and calling appendChild
 * causes it to reload.
 *
 * Meant to be embedded inside of an HTML page, as in markup-view.xhtml.
 *
 * @param HTMLDocument htmlDocument
 *        The document to attach the editor to.  Will also use this
 *        document as a basis for listening resize events.
 */
function HTMLEditor(htmlDocument)
{
  this.doc = htmlDocument;
  this.container = this.doc.createElement("div");
  this.container.className = "html-editor theme-body";
  this.container.style.display = "none";
  this.editorInner = this.doc.createElement("div");
  this.editorInner.className = "html-editor-inner";
  this.container.appendChild(this.editorInner);

  this.doc.body.appendChild(this.container);
  this.hide = this.hide.bind(this);
  this.refresh = this.refresh.bind(this);

  EventEmitter.decorate(this);

  this.doc.defaultView.addEventListener("resize",
    this.refresh, true);

  let config = {
    mode: Editor.modes.html,
    lineWrapping: true,
    styleActiveLine: false,
    extraKeys: {},
    theme: "mozilla markup-view"
  };

  config.extraKeys[ctrl("Enter")] = this.hide;
  config.extraKeys["Esc"] = this.hide.bind(this, false);

  this.container.addEventListener("click", this.hide, false);
  this.editorInner.addEventListener("click", stopPropagation, false);
  this.editor = new Editor(config);

  this.editor.appendTo(this.editorInner).then(() => {
    this.hide(false);
  }).then(null, (err) => console.log(err.message));
}

HTMLEditor.prototype = {

  /**
   * Need to refresh position by manually setting CSS values, so this will
   * need to be called on resizes and other sizing changes.
   */
  refresh: function() {
    let element = this._attachedElement;

    if (element) {
      this.container.style.top = element.offsetTop + "px";
      this.container.style.left = element.offsetLeft + "px";
      this.container.style.width = element.offsetWidth + "px";
      this.container.style.height = element.parentNode.offsetHeight + "px";
      this.editor.refresh();
    }
  },

  /**
   * Anchor the editor to a particular element.
   *
   * @param DOMNode element
   *        The element that the editor will be anchored to.
   *        Should belong to the HTMLDocument passed into the constructor.
   */
  _attach: function(element)
  {
    this._detach();
    this._attachedElement = element;
    element.classList.add("html-editor-container");
    this.refresh();
  },

  /**
   * Unanchor the editor from an element.
   */
  _detach: function()
  {
    if (this._attachedElement) {
      this._attachedElement.classList.remove("html-editor-container");
      this._attachedElement = undefined;
    }
  },

  /**
   * Anchor the editor to a particular element, and show the editor.
   *
   * @param DOMNode element
   *        The element that the editor will be anchored to.
   *        Should belong to the HTMLDocument passed into the constructor.
   * @param string text
   *        Value to set the contents of the editor to
   * @param function cb
   *        The function to call when hiding
   */
  show: function(element, text)
  {
    if (this._visible) {
      return;
    }

    this._originalValue = text;
    this.editor.setText(text);
    this._attach(element);
    this.container.style.display = "flex";
    this._visible = true;

    this.editor.refresh();
    this.editor.focus();
  },

  /**
   * Hide the editor, optionally committing the changes
   *
   * @param bool shouldCommit
   *             A change will be committed by default.  If this param
   *             strictly equals false, no change will occur.
   */
  hide: function(shouldCommit)
  {
    if (!this._visible) {
      return;
    }

    this.container.style.display = "none";
    this._detach();

    let newValue = this.editor.getText();
    let valueHasChanged = this._originalValue !== newValue;
    let preventCommit = shouldCommit === false || !valueHasChanged;
    this.emit("popup-hidden", !preventCommit, newValue);
    this._originalValue = undefined;
    this._visible = undefined;
  },

  /**
   * Destroy this object and unbind all event handlers
   */
  destroy: function()
  {
    this.doc.defaultView.removeEventListener("resize",
      this.refresh, true);
    this.container.removeEventListener("click", this.hide, false);
    this.editorInner.removeEventListener("click", stopPropagation, false);

    this.hide(false);
    this.container.parentNode.removeChild(this.container);
  }
};