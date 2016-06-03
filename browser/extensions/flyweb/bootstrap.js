/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Console",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "gFlyWebBundle", function() {
  return Services.strings.createBundle("chrome://flyweb/locale/flyweb.properties");
});

function install(aData, aReason) {}

function uninstall(aData, aReason) {}

function startup(aData, aReason) {
  FlyWebView.init();
}

function shutdown(aData, aReason) {
  FlyWebView.uninit();
}

let gDiscoveryManagerInstance;

class DiscoveryManager {
  constructor(aWindow) {
    this._discoveryManager = new aWindow.FlyWebDiscoveryManager();
  }

  start(callback) {
    if (!this._id) {
      this._id = this._discoveryManager.startDiscovery(this);
    }

    this._callback = callback;
  }

  stop() {
    this._discoveryManager.stopDiscovery(this._id);

    this._id = null;
  }

  pairWith(serviceId, callback) {
    this._discoveryManager.pairWithService(serviceId, {
      pairingSucceeded(service) {
        callback(service);
      },

      pairingFailed(error) {
        console.error("FlyWeb failed to pair with service " + serviceId, error);
      }
    });
  }

  onDiscoveredServicesChanged(services) {
    if (!this._id || !this._callback) {
      return;
    }

    this._callback(services);
  }
}

let FlyWebView = {
  init() {
    // Create widget and add it to the menu panel.
    CustomizableUI.createWidget({
      id: "flyweb-button",
      type: "view",
      viewId: "flyweb-panel",
      label: gFlyWebBundle.GetStringFromName("flyweb-button.label"),
      tooltiptext: gFlyWebBundle.GetStringFromName("flyweb-button.tooltiptext"),

      onBeforeCreated(aDocument) {
        let panel = aDocument.createElement("panelview");
        panel.id = "flyweb-panel";
        panel.setAttribute("class", "PanelUI-subView");
        panel.setAttribute("flex", "1");

        let label = aDocument.createElement("label");
        label.setAttribute("class", "panel-subview-header");
        label.setAttribute("value", gFlyWebBundle.GetStringFromName("flyweb-button.label"));

        let empty = aDocument.createElement("description");
        empty.id = "flyweb-items-empty";
        empty.setAttribute("mousethrough", "always");
        empty.textContent = gFlyWebBundle.GetStringFromName("flyweb-items-empty");

        let items = aDocument.createElement("vbox");
        items.id = "flyweb-items";
        items.setAttribute("class", "panel-subview-body");

        panel.appendChild(label);
        panel.appendChild(empty);
        panel.appendChild(items);

        panel.addEventListener("command", this);

        aDocument.getElementById("PanelUI-multiView").appendChild(panel);

        this._sheetURI = Services.io.newURI("chrome://flyweb/skin/flyweb.css", null, null);
        aDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).
            getInterface(Ci.nsIDOMWindowUtils).loadSheet(this._sheetURI, 1);
      },

      onDestroyed(aDocument) {
        aDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).
            getInterface(Ci.nsIDOMWindowUtils).removeSheet(this._sheetURI, 1);
      },

      onViewShowing(aEvent) {
        let doc = aEvent.target.ownerDocument;

        let panel = doc.getElementById("flyweb-panel");
        let items = doc.getElementById("flyweb-items");
        let empty = doc.getElementById("flyweb-items-empty");

        if (!gDiscoveryManagerInstance) {
          gDiscoveryManagerInstance = new DiscoveryManager(doc.defaultView);
        }

        gDiscoveryManagerInstance.start((services) => {
          while (items.firstChild) {
            items.firstChild.remove();
          }

          let fragment = doc.createDocumentFragment();

          for (let service of services) {
            let button = doc.createElement("toolbarbutton");
            button.setAttribute("class", "subviewbutton cui-withicon");
            button.setAttribute("label", service.displayName);
            button.setAttribute("data-service-id", service.serviceId);
            fragment.appendChild(button);
          }

          items.appendChild(fragment);

          empty.hidden = services.length > 0;
        });
      },

      onViewHiding(aEvent) {
        gDiscoveryManagerInstance.stop();
      },

      handleEvent(aEvent) {
        if (aEvent.type === "command") {
          let serviceId = aEvent.target.getAttribute("data-service-id");
          gDiscoveryManagerInstance.pairWith(serviceId, (service) => {
            aEvent.view.openUILinkIn(service.uiUrl, "tab");
          });
        }
      }
    });
  },

  uninit() {
    CustomizableUI.destroyWidget("flyweb-button");
  }
};
