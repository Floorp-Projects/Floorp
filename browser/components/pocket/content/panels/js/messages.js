/* global RPMRemoveMessageListener:false, RPMAddMessageListener:false, RPMSendAsyncMessage:false */

var pktPanelMessaging = (function() {
  function removeMessageListener(messageId, callback) {
    RPMRemoveMessageListener(messageId, callback);
  }

  function addMessageListener(messageId, callback = () => {}) {
    RPMAddMessageListener(messageId, callback);
  }

  function sendMessage(messageId, payload = {}, callback) {
    if (callback) {
      // If we expect something back, we use RPMSendAsyncMessage and not RPMSendQuery.
      // Even though RPMSendQuery returns something, our frame could be closed at any moment,
      // and we don't want to close a RPMSendQuery promise loop unexpectedly.
      // So instead we setup a response event.
      const responseMessageId = `${messageId}_response`;
      var responseListener = function(responsePayload) {
        callback(responsePayload);
        removeMessageListener(responseMessageId, responseListener);
      };

      addMessageListener(responseMessageId, responseListener);
    }

    // Send message
    RPMSendAsyncMessage(messageId, payload);
  }

  function log() {
    RPMSendAsyncMessage("PKT_log", arguments);
  }

  /**
   * Public functions
   */
  return {
    log,
    addMessageListener,
    sendMessage,
  };
})();
