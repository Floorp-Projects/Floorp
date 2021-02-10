/* global RPMRemoveMessageListener:false, RPMAddMessageListener:false, RPMSendAsyncMessage:false */

var pktPanelMessaging = (function() {
  function panelIdFromURL(url) {
    var panelId = url.match(/panelId=([\w|\d|\.]*)&?/);
    if (panelId && panelId.length > 1) {
      return panelId[1];
    }

    return 0;
  }

  function removeMessageListener(messageId, panelId, callback) {
    RPMRemoveMessageListener(`${messageId}_${panelId}`, callback);
  }

  function addMessageListener(messageId, panelId, callback = () => {}) {
    RPMAddMessageListener(`${messageId}_${panelId}`, callback);
  }

  function sendMessage(messageId, panelId, payload = {}, callback) {
    // Payload needs to be an object in format:
    // { panelId: panelId, payload: {} }
    var messagePayload = {
      panelId,
      payload,
    };

    if (callback) {
      // If we expect something back, we use RPMSendAsyncMessage and not RPMSendQuery.
      // Even though RPMSendQuery returns something, our frame could be closed at any moment,
      // and we don't want to close a RPMSendQuery promise loop unexpectedly.
      // So instead we setup a response event.
      const responseMessageId = `${messageId}_response`;
      var responseListener = function(responsePayload) {
        callback(responsePayload);
        removeMessageListener(responseMessageId, panelId, responseListener);
      };

      addMessageListener(responseMessageId, panelId, responseListener);
    }

    // Send message
    RPMSendAsyncMessage(messageId, messagePayload);
  }

  function log() {
    RPMSendAsyncMessage("PKT_log", arguments);
  }

  /**
   * Public functions
   */
  return {
    log,
    panelIdFromURL,
    addMessageListener,
    sendMessage,
  };
})();
