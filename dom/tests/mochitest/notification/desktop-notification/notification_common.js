const MOCK_ALERTS_CID = SpecialPowers.wrap(SpecialPowers.Components).ID("{48068bc2-40ab-4904-8afd-4cdfb3a385f3}");
const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";

const MOCK_SYSTEM_ALERTS_CID = SpecialPowers.wrap(SpecialPowers.Components).ID("{e86d888c-e41b-4b78-9104-2f2742a532de}");
const SYSTEM_ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/system-alerts-service;1";

var registrar = SpecialPowers.wrap(SpecialPowers.Components).manager.
  QueryInterface(SpecialPowers.Ci.nsIComponentRegistrar);

var mockAlertsService = {
  showAlert: function(alert, alertListener) {
    // probably should do this async....
    SpecialPowers.wrap(alertListener).observe(null, "alertshow", alert.cookie);

    if (SpecialPowers.getBoolPref("notification.prompt.testing.click_on_notification") == true) {
       SpecialPowers.wrap(alertListener).observe(null, "alertclickcallback", alert.cookie);
    }

    SpecialPowers.wrap(alertListener).observe(null, "alertfinished", alert.cookie);
  },

  showAlertNotification: function(imageUrl, title, text, textClickable,
                                  cookie, alertListener, name, bidi,
                                  lang, data) {
    return this.showAlert({
      cookie: cookie
    }, alertListener);
  },

  QueryInterface: function(aIID) {
    if (SpecialPowers.wrap(aIID).equals(SpecialPowers.Ci.nsISupports) ||
        SpecialPowers.wrap(aIID).equals(SpecialPowers.Ci.nsIAlertsService)) {
      return this;
    }
    throw SpecialPowers.Components.results.NS_ERROR_NO_INTERFACE;
  },

  createInstance: function(aOuter, aIID) {
    if (aOuter != null) {
      throw SpecialPowers.Components.results.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  }
};
mockAlertsService = SpecialPowers.wrapCallbackObject(mockAlertsService);

function setup_notifications(allowPrompt, forceClick, callback) {
  SpecialPowers.pushPrefEnv({'set': [["notification.prompt.testing", true],
                                     ["notification.prompt.testing.allow", allowPrompt],
                                     ["notification.prompt.testing.click_on_notification", forceClick]]},
                            callback);

  registrar.registerFactory(MOCK_SYSTEM_ALERTS_CID, "system alerts service",
                            SYSTEM_ALERTS_SERVICE_CONTRACT_ID,
                            mockAlertsService);

  registrar.registerFactory(MOCK_ALERTS_CID, "alerts service",
                            ALERTS_SERVICE_CONTRACT_ID,
                            mockAlertsService);
}

function reset_notifications() {
  registrar.unregisterFactory(MOCK_SYSTEM_ALERTS_CID, mockAlertsService);
  registrar.unregisterFactory(MOCK_ALERTS_CID, mockAlertsService);
}

function is_feature_enabled() {
  return navigator.mozNotification && SpecialPowers.getBoolPref("notification.feature.enabled");
}

