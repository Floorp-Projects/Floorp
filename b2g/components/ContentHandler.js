/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const PDF_CONTENT_TYPE = "application/pdf";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"]
           .getService(Ci.nsIMessageSender);
});

function debug(aMsg) {
  //dump("--*-- ContentHandler: " + aMsg + "\n");
}

const NS_ERROR_WONT_HANDLE_CONTENT = 0x805d0001;

let ActivityContentFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return new ActivityContentHandler().QueryInterface(iid);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
}

function ActivityContentHandler() {
}

ActivityContentHandler.prototype = {
  handleContent: function handleContent(aMimetype, aContext, aRequest) {
    if (!(aRequest instanceof Ci.nsIChannel))
      throw NS_ERROR_WONT_HANDLE_CONTENT;

    let detail = {
      "type": aMimetype,
      "url": aRequest.URI.spec
    };
    cpmm.sendAsyncMessage("content-handler", detail);

    aRequest.cancel(Cr.NS_BINDING_ABORTED);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentHandler])
}

function ContentHandler() {
  this.classIdMap = {};
}

ContentHandler.prototype = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "app-startup") {
      // We only want to register these from content processes.
      let appInfo = Cc["@mozilla.org/xre/app-info;1"];
      if (appInfo.getService(Ci.nsIXULRuntime)
          .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
        return;
      }
    }

    cpmm.addMessageListener("Activities:RegisterContentTypes", this);
    cpmm.addMessageListener("Activities:UnregisterContentTypes", this);
    cpmm.sendAsyncMessage("Activities:GetContentTypes", { });
  },

  /**
    * Do the component registration for a content type.
    * We only need to register one component per content type, even if several
    * apps provide it, so we keep track of the number of providers for each
    * content type.
    */
  registerContentHandler: function registerContentHandler(aContentType) {
    debug("Registering " + aContentType);

    // We already have a provider for this content type, just increase the
    // tracking count.
    if (this.classIdMap[aContentType]) {
      this.classIdMap[aContentType].count++;
      return;
    }

    let contractID = "@mozilla.org/uriloader/content-handler;1?type=" +
                     aContentType;
    let uuidGen = Cc["@mozilla.org/uuid-generator;1"]
                    .getService(Ci.nsIUUIDGenerator);
    let id = Components.ID(uuidGen.generateUUID().toString());
    this.classIdMap[aContentType] = { count: 1, id: id };
    let cr = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    cr.registerFactory(Components.ID(id), "Activity Content Handler", contractID,
                       ActivityContentFactory);
  },

  /**
    * Do the component unregistration for a content type.
    */
  unregisterContentHandler: function registerContentHandler(aContentType) {
    debug("Unregistering " + aContentType);

    let record = this.classIdMap[aContentType];
    if (!record) {
      return;
    }

    // Bail out if we still have providers left for this content type.
    if (--record.count > 0) {
      return;
    }

    let contractID = "@mozilla.org/uriloader/content-handler;1?type=" +
                     aContentType;
    let cr = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    cr.unregisterFactory(record.id, ActivityContentFactory);
    delete this.classIdMap[aContentType]
  },

  receiveMessage: function(aMessage) {
    let data = aMessage.data;

    switch (aMessage.name) {
      case "Activities:RegisterContentTypes":
        data.contentTypes.forEach(this.registerContentHandler, this);
        break;
      case "Activities:UnregisterContentTypes":
        data.contentTypes.forEach(this.unregisterContentHandler, this);
        break;
    }
  },

  classID: Components.ID("{d18d0216-d50c-11e1-ba54-efb18d0ef0ac}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentHandler,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentHandler]);
