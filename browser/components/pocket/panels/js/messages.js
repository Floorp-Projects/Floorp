// Documentation of methods used here are at:
// https://developer.mozilla.org/en-US/Add-ons/Code_snippets/Interaction_between_privileged_and_non-privileged_pages

var Messaging = (function() {

	function addMessageListener(messageId, callback) {

		document.addEventListener('PKT_' + messageId, function(e) {

			callback(JSON.parse(e.target.getAttribute("payload"))[0]);

			// TODO: Figure out why e.target.parentNode is null
			// e.target.parentNode.removeChild(e.target);

		},false);

	}

	function removeMessageListener(messageId, callback) {

		document.removeEventListener('PKT_' + messageId, callback);

	}

	function sendMessage(messageId, payload, callback) {

		// Create a callback to listen for a response
		if (callback) {
			 // Message response id is messageId + "Response"
	        var messageResponseId = messageId + "Response";
	        var responseListener = function(responsePayload) {
	            callback(responsePayload);
	            removeMessageListener(messageResponseId, responseListener);
	        }

	        addMessageListener(messageResponseId, responseListener);
	    }

	    // Send message
		var element = document.createElement("PKTMessageFromPanelElement");
		element.setAttribute("payload", JSON.stringify([payload]));
		document.documentElement.appendChild(element);

		var evt = document.createEvent("Events");
		evt.initEvent('PKT_'+messageId, true, false);
		element.dispatchEvent(evt);
	}


    /**
     * Public functions
     */
    return {
        addMessageListener : addMessageListener,
        removeMessageListener : removeMessageListener,
        sendMessage: sendMessage
    };
}());