/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces;
let Cc = Components.classes;

this.kXLinkNamespace = "http://www.w3.org/1999/xlink";

dump("### ContextMenuHandler.js loaded\n");

var ContextMenuHandler = {
  _types: [],
  _previousState: null,

  init: function ch_init() {
    // Events we catch from content during the bubbling phase
    addEventListener("contextmenu", this, false);
    addEventListener("pagehide", this, false);

    // Messages we receive from browser
    // Command sent over from browser that only we can handle.
    addMessageListener("Browser:ContextCommand", this, false);

    this.popupNode = null;
  },

  handleEvent: function ch_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "contextmenu":
        this._onContentContextMenu(aEvent);
        break;
      case "pagehide":
        this.reset();
        break;
    }
  },

  receiveMessage: function ch_receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Browser:ContextCommand":
        this._onContextCommand(aMessage);
      break;
    }
  },

  /*
   * Handler for commands send over from browser's ContextCommands.js
   * in response to certain context menu actions only we can handle.
   */
  _onContextCommand: function _onContextCommand(aMessage) {
    let node = this.popupNode;
    let command = aMessage.json.command;

    switch (command) {
      case "cut":
        this._onCut();
        break;

      case "copy":
        this._onCopy();
        break;

      case "paste":
        this._onPaste();
        break;

      case "select-all":
        this._onSelectAll();
        break;

      case "copy-image-contents":
        this._onCopyImage();
        break;
    }
  },

  /******************************************************
   * Event handlers
   */

  reset: function ch_reset() {
    this.popupNode = null;
    this._target = null;
  },

  // content contextmenu handler
  _onContentContextMenu: function _onContentContextMenu(aEvent) {
    if (aEvent.defaultPrevented)
      return;

    // Don't let these bubble up to input.js
    aEvent.stopPropagation();
    aEvent.preventDefault();

    this._processPopupNode(aEvent.originalTarget, aEvent.clientX,
                           aEvent.clientY, aEvent.mozInputSource);
  },

  /******************************************************
   * ContextCommand handlers
   */

  _onSelectAll: function _onSelectAll() {
    if (Util.isTextInput(this._target)) {
      // select all text in the input control
      this._target.select();
    } else if (Util.isEditableContent(this._target)) {
      this._target.ownerDocument.execCommand("selectAll", false);
    } else {
      // select the entire document
      content.getSelection().selectAllChildren(content.document);
    }
    this.reset();
  },

  _onPaste: function _onPaste() {
    // paste text if this is an input control
    if (Util.isTextInput(this._target)) {
      let edit = this._target.QueryInterface(Ci.nsIDOMNSEditableElement);
      if (edit) {
        edit.editor.paste(Ci.nsIClipboard.kGlobalClipboard);
      } else {
        Util.dumpLn("error: target element does not support nsIDOMNSEditableElement");
      }
    } else if (Util.isEditableContent(this._target)) {
      try {
        this._target.ownerDocument.execCommand("paste",
                                               false,
                                               Ci.nsIClipboard.kGlobalClipboard);
      } catch (ex) {
        dump("ContextMenuHandler: exception pasting into contentEditable: " + ex.message + "\n");
      }
    }
    this.reset();
  },

  _onCopyImage: function _onCopyImage() {
    Util.copyImageToClipboard(this._target);
  },

  _onCut: function _onCut() {
    if (Util.isTextInput(this._target)) {
      let edit = this._target.QueryInterface(Ci.nsIDOMNSEditableElement);
      if (edit) {
        edit.editor.cut();
      } else {
        Util.dumpLn("error: target element does not support nsIDOMNSEditableElement");
      }
    } else if (Util.isEditableContent(this._target)) {
      try {
        this._target.ownerDocument.execCommand("cut", false);
      } catch (ex) {
        dump("ContextMenuHandler: exception cutting from contentEditable: " + ex.message + "\n");
      }
    }
    this.reset();
  },

  _onCopy: function _onCopy() {
    if (Util.isTextInput(this._target)) {
      let edit = this._target.QueryInterface(Ci.nsIDOMNSEditableElement);
      if (edit) {
        edit.editor.copy();
      } else {
        Util.dumpLn("error: target element does not support nsIDOMNSEditableElement");
      }
    } else if (Util.isEditableContent(this._target)) {
      try {
        this._target.ownerDocument.execCommand("copy", false);
      } catch (ex) {
        dump("ContextMenuHandler: exception copying from contentEditable: " +
          ex.message + "\n");
      }
    } else {
      let selectionText = this._previousState.string;

      Cc["@mozilla.org/widget/clipboardhelper;1"]
        .getService(Ci.nsIClipboardHelper).copyString(selectionText);
    }
    this.reset();
  },

  /******************************************************
   * Utility routines
   */

  /*
   * _processPopupNode - Generate and send a Content:ContextMenu message
   * to browser detailing the underlying content types at this.popupNode.
   * Note the event we receive targets the sub frame (if there is one) of
   * the page.
   */
  _processPopupNode: function _processPopupNode(aPopupNode, aX, aY, aInputSrc) {
    if (!aPopupNode)
      return;

    let { targetWindow: targetWindow,
          offsetX: offsetX,
          offsetY: offsetY } =
      Util.translateToTopLevelWindow(aPopupNode);

    let popupNode = this.popupNode = aPopupNode;
    let imageUrl = "";

    let state = {
      types: [],
      label: "",
      linkURL: "",
      linkTitle: "",
      linkProtocol: null,
      mediaURL: "",
      contentType: "",
      contentDisposition: "",
      string: "",
    };
    let uniqueStateTypes = new Set();

    // Do checks for nodes that never have children.
    if (popupNode.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
      // See if the user clicked on an image.
      if (popupNode instanceof Ci.nsIImageLoadingContent && popupNode.currentURI) {
        uniqueStateTypes.add("image");
        state.label = state.mediaURL = popupNode.currentURI.spec;
        imageUrl = state.mediaURL;
        this._target = popupNode;

        // Retrieve the type of image from the cache since the url can fail to
        // provide valuable informations
        try {
          let imageCache = Cc["@mozilla.org/image/cache;1"].getService(Ci.imgICache);
          let props = imageCache.findEntryProperties(popupNode.currentURI,
                                                     content.document.characterSet);
          if (props) {
            state.contentType = String(props.get("type", Ci.nsISupportsCString));
            state.contentDisposition = String(props.get("content-disposition",
                                                        Ci.nsISupportsCString));
          }
        } catch (ex) {
          Util.dumpLn(ex.message);
          // Failure to get type and content-disposition off the image is non-fatal
        }
      }
    }

    let elem = popupNode;
    let isText = false;
    let isEditableText = false;

    while (elem) {
      if (elem.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
        // is the target a link or a descendant of a link?
        if (Util.isLink(elem)) {
          // If this is an image that links to itself, don't include both link and
          // image otpions.
          if (imageUrl == this._getLinkURL(elem)) {
            elem = elem.parentNode;
            continue;
          }

          uniqueStateTypes.add("link");
          state.label = state.linkURL = this._getLinkURL(elem);
          linkUrl = state.linkURL;
          state.linkTitle = popupNode.textContent || popupNode.title;
          state.linkProtocol = this._getProtocol(this._getURI(state.linkURL));
          // mark as text so we can pickup on selection below
          isText = true;
          break;
        }
        // is the target contentEditable (not just inheriting contentEditable)
        // or the entire document in designer mode.
        else if (elem.contentEditable == "true" ||
                 Util.isOwnerDocumentInDesignMode(elem)) {
          this._target = elem;
          isEditableText = true;
          isText = true;
          uniqueStateTypes.add("input-text");

          if (elem.textContent.length) {
            uniqueStateTypes.add("selectable");
          } else {
            uniqueStateTypes.add("input-empty");
          }
          break;
        }
        // is the target a text input
        else if (Util.isTextInput(elem)) {
          this._target = elem;
          isEditableText = true;
          uniqueStateTypes.add("input-text");

          let selectionStart = elem.selectionStart;
          let selectionEnd = elem.selectionEnd;

          // Don't include "copy" for password fields.
          if (!(elem instanceof Ci.nsIDOMHTMLInputElement) || elem.mozIsTextField(true)) {
            // If there is a selection add cut and copy
            if (selectionStart != selectionEnd) {
              uniqueStateTypes.add("cut");
              uniqueStateTypes.add("copy");
              state.string = elem.value.slice(selectionStart, selectionEnd);
            } else if (elem.value && elem.textLength) {
              // There is text and it is not selected so add selectable items
              uniqueStateTypes.add("selectable");
              state.string = elem.value;
            }
          }

          if (!elem.textLength) {
            uniqueStateTypes.add("input-empty");
          }
          break;
        }
        // is the target an element containing text content
        else if (Util.isText(elem)) {
          isText = true;
        }
        // is the target a media element
        else if (elem instanceof Ci.nsIDOMHTMLMediaElement ||
                   elem instanceof Ci.nsIDOMHTMLVideoElement) {
          state.label = state.mediaURL = (elem.currentSrc || elem.src);
          uniqueStateTypes.add((elem.paused || elem.ended) ?
            "media-paused" : "media-playing");
          if (elem instanceof Ci.nsIDOMHTMLVideoElement) {
            uniqueStateTypes.add("video");
          }
        }
      }

      elem = elem.parentNode;
    }

    // Over arching text tests
    if (isText) {
      // If this is text and has a selection, we want to bring
      // up the copy option on the context menu.
      let selection = targetWindow.getSelection();
      if (selection && this._tapInSelection(selection, aX, aY)) {
        state.string = targetWindow.getSelection().toString();
        uniqueStateTypes.add("copy");
        uniqueStateTypes.add("selected-text");
        if (isEditableText) {
          uniqueStateTypes.add("cut");
        }
      } else {
        // Add general content text if this isn't anything specific
        if (!(
            uniqueStateTypes.has("image") ||
            uniqueStateTypes.has("media") ||
            uniqueStateTypes.has("video") ||
            uniqueStateTypes.has("link") ||
            uniqueStateTypes.has("input-text")
        )) {
          uniqueStateTypes.add("content-text");
        }
      }
    }

    // Is paste applicable here?
    if (isEditableText) {
      let flavors = ["text/unicode"];
      let cb = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
      let hasData = cb.hasDataMatchingFlavors(flavors,
                                              flavors.length,
                                              Ci.nsIClipboard.kGlobalClipboard);
      // add paste if there's data
      if (hasData && !elem.readOnly) {
        uniqueStateTypes.add("paste");
      }
    }
    // populate position and event source
    state.xPos = offsetX + aX;
    state.yPos = offsetY + aY;
    state.source = aInputSrc;

    for (let i = 0; i < this._types.length; i++)
      if (this._types[i].handler(state, popupNode))
        uniqueStateTypes.add(this._types[i].name);

    state.types = [type for (type of uniqueStateTypes)];
    this._previousState = state;

    sendAsyncMessage("Content:ContextMenu", state);
  },

  _tapInSelection: function (aSelection, aX, aY) {
    if (!aSelection || !aSelection.rangeCount) {
      return false;
    }
    for (let idx = 0; idx < aSelection.rangeCount; idx++) {
      let range = aSelection.getRangeAt(idx);
      let rect = range.getBoundingClientRect();
      if (Util.pointWithinDOMRect(aX, aY, rect)) {
        return true;
      }
    }
    return false;
  },

  _getLinkURL: function ch_getLinkURL(aLink) {
    let href = aLink.href;
    if (href)
      return href;

    href = aLink.getAttributeNS(kXLinkNamespace, "href");
    if (!href || !href.match(/\S/)) {
      // Without this we try to save as the current doc,
      // for example, HTML case also throws if empty
      throw "Empty href";
    }

    return Util.makeURLAbsolute(aLink.baseURI, href);
  },

  _getURI: function ch_getURI(aURL) {
    try {
      return Util.makeURI(aURL);
    } catch (ex) { }

    return null;
  },

  _getProtocol: function ch_getProtocol(aURI) {
    if (aURI)
      return aURI.scheme;
    return null;
  },

  /**
   * For add-ons to add new types and data to the ContextMenu message.
   *
   * @param aName A string to identify the new type.
   * @param aHandler A function that takes a state object and a target element.
   *    If aHandler returns true, then aName will be added to the list of types.
   *    The function may also modify the state object.
   */
  registerType: function registerType(aName, aHandler) {
    this._types.push({name: aName, handler: aHandler});
  },

  /** Remove all handlers registered for a given type. */
  unregisterType: function unregisterType(aName) {
    this._types = this._types.filter(function(type) type.name != aName);
  }
};
this.ContextMenuHandler = ContextMenuHandler;

ContextMenuHandler.init();
