/*
 * BrowserTouchHandler
 *
 * Receives touch events from our input event capturing in input.js
 * and relays appropriate events to content. Also handles requests
 * from content for UI.
 */

const BrowserTouchHandler = {
  _debugEvents: false,

  init: function init() {
    // Misc. events we catch during the bubbling phase
    document.addEventListener("PopupChanged", this, false);
    document.addEventListener("CancelTouchSequence", this, false);

    // Messages sent from content.js
    messageManager.addMessageListener("Content:ContextMenu", this);
    messageManager.addMessageListener("Content:SelectionCaret", this);
  },

  // Content forwarding the contextmenu command
  _onContentContextMenu: function _onContentContextMenu(aMessage) {
    // Note, target here is the target of the message manager message,
    // usually the browser.
    // Touch input selection handling
    if (!InputSourceHelper.isPrecise &&
        !SelectionHelperUI.isActive &&
        SelectionHelperUI.canHandleContextMenuMsg(aMessage)) {
      SelectionHelperUI.openEditSession(aMessage.target,
                                        aMessage.json.xPos,
                                        aMessage.json.yPos);
      return;
    }

    // Check to see if we have context menu item(s) that apply to what
    // was clicked on.
    let contextInfo = { name: aMessage.name,
                        json: aMessage.json,
                        target: aMessage.target };
    if (ContextMenuUI.showContextMenu(contextInfo)) {
      let event = document.createEvent("Events");
      event.initEvent("CancelTouchSequence", true, false);
      document.dispatchEvent(event);
    } else {
      // Send the MozEdgeUIGesture to input.js to
      // toggle the context ui.
      let event = document.createEvent("Events");
      event.initEvent("MozEdgeUICompleted", true, false);
      window.dispatchEvent(event);
    }
  },

  /*
   * Called when Content wants to initiate selection management
   * due to a tap in a form input.
   */
  _onCaretSelectionStarted: function _onCaretSelectionStarted(aMessage) {
    SelectionHelperUI.attachToCaret(aMessage.target,
                                    aMessage.json.xPos,
                                    aMessage.json.yPos);
  },

  /*
   * Events
   */

  handleEvent: function handleEvent(aEvent) {
    // ignore content events we generate
    if (this._debugEvents)
      Util.dumpLn("BrowserTouchHandler:", aEvent.type);

    switch (aEvent.type) {
      case "PopupChanged":
      case "CancelTouchSequence":
        if (!aEvent.detail)
          ContextMenuUI.reset();
        break;
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    if (this._debugEvents) Util.dumpLn("BrowserTouchHandler:", aMessage.name);
    switch (aMessage.name) {
      // Content forwarding the contextmenu command
      case "Content:ContextMenu":
        this._onContentContextMenu(aMessage);
        break;
      case "Content:SelectionCaret":
        this._onCaretSelectionStarted(aMessage);
        break;
    }
  },
};
