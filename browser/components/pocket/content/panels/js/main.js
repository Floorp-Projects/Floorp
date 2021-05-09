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
    // Mutation observer isn't always enough for fast loading, static pages.
    // Sometimes the mutation observer fires before the page is totally visible.
    // In this case, the resize tries to fire with 0 height,
    // and because it's a static page, it only does one mutation.
    // So in this case, we have a backup intersection observer that fires when
    // the page is first visible, and thus, the page is going to guarantee a height.
    this.setupIntersectionObserver();

    this.inited = true;
  },

  addMessageListener(messageId, callback) {
    pktPanelMessaging.addMessageListener(messageId, this.panelId, callback);
  },

  sendMessage(messageId, payload, callback) {
    pktPanelMessaging.sendMessage(messageId, this.panelId, payload, callback);
  },

  resizeParent() {
    let clientHeight = document.body.clientHeight;
    if (this.overlay.tagsDropdownOpen) {
      clientHeight = Math.max(clientHeight, 252);
    }

    // We can ignore 0 height here.
    // We rely on intersection observer to do the
    // resize for 0 height loads.
    if (clientHeight) {
      thePKT_PANEL.sendMessage("PKT_resizePanel", {
        width: document.body.clientWidth,
        height: clientHeight,
      });
    }
  },

  setupIntersectionObserver() {
    const observer = new IntersectionObserver(entries => {
      if (entries.find(e => e.isIntersecting)) {
        this.resizeParent();
        observer.unobserve(document.body);
      }
    });
    observer.observe(document.body);
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
            this.resizeParent();
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
