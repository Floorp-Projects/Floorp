// Documentation of methods used here are at:
// https://developer.mozilla.org/en-US/Add-ons/Code_snippets/Interaction_between_privileged_and_non-privileged_pages

var pktPanelMessaging = (function() {

 function panelIdFromURL(url) {
   var panelId = url.match(/panelId=([\w|\d|\.]*)&?/);
        if (panelId && panelId.length > 1) {
            return panelId[1];
        }

        return 0;
 }

 function prefixedMessageId(messageId) {
   return 'PKT_' + messageId;
 }

 function panelPrefixedMessageId(panelId, messageId) {
   return prefixedMessageId(panelId + '_' + messageId);
 }

 function addMessageListener(panelId, messageId, callback) {
   document.addEventListener(panelPrefixedMessageId(panelId, messageId), function(e) {

			callback(JSON.parse(e.target.getAttribute("payload"))[0]);

			// TODO: Figure out why e.target.parentNode is null
			// e.target.parentNode.removeChild(e.target);

		}, false);

	}

	function removeMessageListener(panelId, messageId, callback) {
   document.removeEventListener(panelPrefixedMessageId(panelId, messageId), callback);
	}

 function sendMessage(panelId, messageId, payload, callback) {
   // Payload needs to be an object in format:
   // { panelId: panelId, data: {} }
   var messagePayload = {
     panelId: panelId,
     data: (payload || {})
   };

		// Create a callback to listen for a response
		if (callback) {
	        var messageResponseId = messageId + "Response";
	        var responseListener = function(responsePayload) {
	            callback(responsePayload);
	            removeMessageListener(panelId, messageResponseId, responseListener);
	        }

	        addMessageListener(panelId, messageResponseId, responseListener);
	    }

	    // Send message
		var element = document.createElement("PKTMessageFromPanelElement");
		element.setAttribute("payload", JSON.stringify([messagePayload]));
		document.documentElement.appendChild(element);

		var evt = document.createEvent("Events");
		evt.initEvent(prefixedMessageId(messageId), true, false);
		element.dispatchEvent(evt);
	}


    /**
     * Public functions
     */
    return {
      panelIdFromURL: panelIdFromURL,
        addMessageListener : addMessageListener,
        removeMessageListener : removeMessageListener,
        sendMessage: sendMessage
    };
}());
