/* global PKT_PANEL_OVERLAY:false */
/* import-globals-from messages.js */

var PKT_PANEL = function() {};

PKT_PANEL.prototype = {
  init() {
    if (this.inited) {
      return;
    }
    this.panelId = pktPanelMessaging.panelIdFromURL(window.location.href);
    this.overlay = new PKT_PANEL_OVERLAY();
    this.setupMutationObserver();

    this.inited = true;
  },

  addMessageListener(messageId, callback) {
    pktPanelMessaging.addMessageListener(messageId, this.panelId, callback);
  },

  sendMessage(messageId, payload, callback) {
    pktPanelMessaging.sendMessage(messageId, this.panelId, payload, callback);
  },

  setupMutationObserver() {
    // Select the node that will be observed for mutations
    const targetNode = document.body;

    // Options for the observer (which mutations to observe)
    const config = { attributes: false, childList: true, subtree: true };

    // Callback function to execute when mutations are observed
    const callback = (mutationList, observer) => {
      mutationList.forEach(mutation => {
        switch (mutation.type) {
          case "childList": {
            /* One or more children have been added to and/or removed
               from the tree.
               (See mutation.addedNodes and mutation.removedNodes.) */
            let clientHeight = document.body.clientHeight;
            if (this.overlay.tagsDropdownOpen) {
              clientHeight = Math.max(clientHeight, 252);
            }
            thePKT_PANEL.sendMessage("PKT_resizePanel", {
              width: document.body.clientWidth,
              height: clientHeight,
            });
            break;
          }
        }
      });
    };

    // Create an observer instance linked to the callback function
    const observer = new MutationObserver(callback);

    // Start observing the target node for configured mutations
    observer.observe(targetNode, config);
  },

  create() {
    this.overlay.create();
  },
};

function onDOMLoaded() {
  if (!window.thePKT_PANEL) {
    var thePKT_PANEL = new PKT_PANEL();
    /* global thePKT_PANEL */
    window.thePKT_PANEL = thePKT_PANEL;
    thePKT_PANEL.init();
  }

  var pocketHost = thePKT_PANEL.overlay.pockethost;
  // send an async message to get string data
  thePKT_PANEL.sendMessage(
    "PKT_initL10N",
    {
      tos: [
        "https://" + pocketHost + "/tos?s=ffi&t=tos&tv=panel_tryit",
        "https://" +
          pocketHost +
          "/privacy?s=ffi&t=privacypolicy&tv=panel_tryit",
      ],
      pockethomeparagraph: ["Pocket"],
    },
    function(resp) {
      const { data } = resp;
      window.pocketStrings = data.strings;
      // Set the writing system direction
      document.documentElement.setAttribute("dir", data.dir);
      window.thePKT_PANEL.create();
    }
  );
}

if (document.readyState != `loading`) {
  onDOMLoaded();
} else {
  document.addEventListener(`DOMContentLoaded`, onDOMLoaded);
}
