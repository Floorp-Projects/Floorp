var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function executeSoon(f)
{
  Services.tm.mainThread.dispatch(f, Ci.nsIThread.DISPATCH_NORMAL);
}

var urlSuffix = "/this/is/the/test/url";

// Content policy / factory implementation for the test
var policyID = Components.ID("{6aadacff-f1f2-46f4-a6db-6d429f884a30}");
var policyName = "@mozilla.org/simpletestpolicy;1";
var policy = {
  // nsISupports implementation
  QueryInterface:
    XPCOMUtils.generateQI([
      Ci.nsISupports,
      Ci.nsIFactory,
      Ci.nsISimpleContentPolicy]),

  // nsIFactory implementation
  createInstance: function(outer, iid) {
    return this.QueryInterface(iid);
  },

  // nsIContentPolicy implementation
  shouldLoad: function(contentType,
                       contentLocation,
                       requestOrigin,
                       frame,
                       isTopLevel,
                       mimeTypeGuess,
                       extra)
  {
    // Remember last content type seen for the test url
    if (contentLocation.spec.endsWith(urlSuffix)) {
      sendAsyncMessage("shouldLoad", {contentType: contentType, isTopLevel: isTopLevel});
      return Ci.nsIContentPolicy.REJECT_REQUEST;
    }

    return Ci.nsIContentPolicy.ACCEPT;
  },

  shouldProcess: function() {
    return Ci.nsIContentPolicy.ACCEPT;
  }
}

// Register content policy
var componentManager = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
componentManager.registerFactory(policyID, "Test simple content policy", policyName, policy);

var categoryManager = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
categoryManager.addCategoryEntry("simple-content-policy", policyName, policyName, false, true);

addMessageListener("finished", () => {
  // Unregister content policy
  categoryManager.deleteCategoryEntry("simple-content-policy", policyName, false);

  executeSoon(function() {
    // Component must be unregistered delayed, otherwise other content
    // policy will not be removed from the category correctly
    componentManager.unregisterFactory(policyID, policy);
  });
});

sendAsyncMessage("ready");
