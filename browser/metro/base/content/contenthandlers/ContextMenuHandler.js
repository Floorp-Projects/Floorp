/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kXLinkNamespace = "http://www.w3.org/1999/xlink";

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
    // InvokeContextAtPoint is sent to us from browser's selection
    // overlay when it traps a contextmenu event. In response we
    // should invoke context menu logic at the point specified.
    addMessageListener("Browser:InvokeContextAtPoint", this, false);

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
      case "Browser:InvokeContextAtPoint":
        this._onContextAtPoint(aMessage);
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

  /*
   * Handler for selection overlay context menu events.
   */
  _onContextAtPoint: function _onContextAtPoint(aMessage) {
    // we need to find popupNode as if the context menu were
    // invoked on underlying content.
    let { element, frameX, frameY } =
      elementFromPoint(aMessage.json.xPos, aMessage.json.yPos);
    this._processPopupNode(element, frameX, frameY,
                           Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH);
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
    * _translateToTopLevelWindow - Given a potential coordinate set within
    * a subframe, translate up to the parent window which is what front
    * end code expect.
    */
  _translateToTopLevelWindow: function _translateToTopLevelWindow(aPopupNode) {
    let offsetX = 0;
    let offsetY = 0;
    let element = aPopupNode;
    while (element &&
           element.ownerDocument &&
           element.ownerDocument.defaultView != content) {
      element = element.ownerDocument.defaultView.frameElement;
      let rect = element.getBoundingClientRect();
      offsetX += rect.left;
      offsetY += rect.top;
    }
    let win = null;
    if (element == aPopupNode)
      win = content;
    else
      win = element.contentDocument.defaultView;
    return { targetWindow: win, offsetX: offsetX, offsetY: offsetY };
  },

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
      this._translateToTopLevelWindow(aPopupNode);

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

    // Do checks for nodes that never have children.
    if (popupNode.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
      // See if the user clicked on an image.
      if (popupNode instanceof Ci.nsIImageLoadingContent && popupNode.currentURI) {
        state.types.push("image");
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

          state.types.push("link");
          state.label = state.linkURL = this._getLinkURL(elem);
          linkUrl = state.linkURL;
          state.linkTitle = popupNode.textContent || popupNode.title;
          state.linkProtocol = this._getProtocol(this._getURI(state.linkURL));
          // mark as text so we can pickup on selection below
          isText = true;
          break;
        } else if (Util.isTextInput(elem)) {
          let selectionStart = elem.selectionStart;
          let selectionEnd = elem.selectionEnd;

          state.types.push("input-text");
          this._target = elem;

          // Don't include "copy" for password fields.
          if (!(elem instanceof Ci.nsIDOMHTMLInputElement) || elem.mozIsTextField(true)) {
            if (selectionStart != selectionEnd) {
              state.types.push("cut");
              state.types.push("copy");
              state.string = elem.value.slice(selectionStart, selectionEnd);
            }
            if (elem.value && (selectionStart > 0 || selectionEnd < elem.textLength)) {
              state.types.push("selectable");
              state.string = elem.value;
            }
          }

          if (!elem.textLength) {
            state.types.push("input-empty");
          }

          let flavors = ["text/unicode"];
          let cb = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
          let hasData = cb.hasDataMatchingFlavors(flavors,
                                                  flavors.length,
                                                  Ci.nsIClipboard.kGlobalClipboard);
          if (hasData && !elem.readOnly) {
            state.types.push("paste");
          }
          break;
        } else if (Util.isText(elem)) {
          isText = true;
        } else if (elem instanceof Ci.nsIDOMHTMLMediaElement ||
                   elem instanceof Ci.nsIDOMHTMLVideoElement) {
          state.label = state.mediaURL = (elem.currentSrc || elem.src);
          state.types.push((elem.paused || elem.ended) ?
            "media-paused" : "media-playing");
          if (elem instanceof Ci.nsIDOMHTMLVideoElement) {
            state.types.push("video");
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
      if (selection && selection.toString().length > 0) {
        state.string = targetWindow.getSelection().toString();
        state.types.push("copy");
        state.types.push("selected-text");
      } else {
        // Add general content text if this isn't anything specific
        if (state.types.indexOf("image") == -1 &&
            state.types.indexOf("media") == -1 &&
            state.types.indexOf("video") == -1 &&
            state.types.indexOf("link") == -1 &&
            state.types.indexOf("input-text") == -1) {
          state.types.push("content-text");
        }
      }
    }

    // populate position and event source
    state.xPos = offsetX + aX;
    state.yPos = offsetY + aY;
    state.source = aInputSrc;

    for (let i = 0; i < this._types.length; i++)
      if (this._types[i].handler(state, popupNode))
        state.types.push(this._types[i].name);

    this._previousState = state;

    sendAsyncMessage("Content:ContextMenu", state);
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

ContextMenuHandler.init();
