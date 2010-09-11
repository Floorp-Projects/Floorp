netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

const FAKE_CID = Components.classes["@mozilla.org/uuid-generator;1"].
    getService(Components.interfaces.nsIUUIDGenerator).generateUUID();

const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";
const ALERTS_SERVICE_CID = Components.ID(Components.classes[ALERTS_SERVICE_CONTRACT_ID].number);

function MockAlertsService() {}

MockAlertsService.prototype = {

    showAlertNotification: function(imageUrl, title, text, textClickable,
                                    cookie, alertListener, name) {
        netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

        // probably should do this async....

        var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
        if (prefs.getBoolPref("notification.prompt.testing.click_on_notification") == true) {
            alertListener.observe(null, "alertclickcallback", cookie);
        }

        alertListener.observe(null, "alertfinished", cookie);
    },

    QueryInterface: function(aIID) {
        netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
        if (aIID.equals(Components.interfaces.nsISupports) ||
            aIID.equals(Components.interfaces.nsIAlertsService))
            return this;
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }
};

var factory = {
    createInstance: function(aOuter, aIID) {
        netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
        if (aOuter != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
        return new MockAlertsService().QueryInterface(aIID);
    }
};

function force_prompt(allow) {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  prefs.setBoolPref("notification.prompt.testing", true);
  prefs.setBoolPref("notification.prompt.testing.allow", allow);

  Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar)
            .registerFactory(FAKE_CID, "",
                             ALERTS_SERVICE_CONTRACT_ID,
                             factory)
}

function reset_prompt() {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  prefs.setBoolPref("notification.prompt.testing", false);
  prefs.setBoolPref("notification.prompt.testing.allow", false);

  Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar)
            .registerFactory(ALERTS_SERVICE_CID, "",
                             ALERTS_SERVICE_CONTRACT_ID,
                             null);
}

function force_click_on_notification(val) {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  prefs.setBoolPref("notification.prompt.testing.click_on_notification", val);
}

