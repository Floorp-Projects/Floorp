var MockServices = (function () {
  "use strict";

  const MOCK_ALERTS_CID = SpecialPowers.wrap(SpecialPowers.Components)
                          .ID("{48068bc2-40ab-4904-8afd-4cdfb3a385f3}");
  const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";

  const MOCK_SYSTEM_ALERTS_CID = SpecialPowers.wrap(SpecialPowers.Components)
                                 .ID("{e86d888c-e41b-4b78-9104-2f2742a532de}");
  const SYSTEM_ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/system-alerts-service;1";

  var registrar = SpecialPowers.wrap(SpecialPowers.Components).manager
                  .QueryInterface(SpecialPowers.Ci.nsIComponentRegistrar);

  var activeAlertNotifications = Object.create(null);

  var activeAppNotifications = Object.create(null);

  var mockAlertsService = {
    showAlertNotification: function(imageUrl, title, text, textClickable,
                                    cookie, alertListener, name) {
      var listener = SpecialPowers.wrap(alertListener);
      activeAlertNotifications[name] = {
        listener: listener,
        cookie: cookie
      };

      // fake async alert show event
      if (listener) {
        setTimeout(function () {
          listener.observe(null, "alertshow", cookie);
        }, 100);
      }

      // ?? SpecialPowers.wrap(alertListener).observe(null, "alertclickcallback", cookie);
    },

    showAppNotification: function(aImageUrl, aTitle, aText, aAlertListener, aDetails) {
      var listener = aAlertListener || (activeAlertNotifications[aDetails.id] ? activeAlertNotifications[aDetails.id].listener : undefined);
      activeAppNotifications[aDetails.id] = {
        observer: listener,
        title: aTitle,
        text: aText,
        manifestURL: aDetails.manifestURL,
        imageURL: aImageUrl,
        lang: aDetails.lang || undefined,
        id: aDetails.id || undefined,
        dbId: aDetails.dbId || undefined,
        dir: aDetails.dir || undefined,
        tag: aDetails.tag || undefined,
        timestamp: aDetails.timestamp || undefined
      };
      this.showAlertNotification(aImageUrl, aTitle, aText, true, "", listener, aDetails.id);
    },

    closeAlert: function(name) {
      var alertNotification = activeAlertNotifications[name];
      if (alertNotification) {
        if (alertNotification.listener) {
          alertNotification.listener.observe(null, "alertfinished", alertNotification.cookie);
        }
        delete activeAlertNotifications[name];
      }

      var appNotification = activeAppNotifications[name];
      if (appNotification) {
        delete activeAppNotifications[name];
      }
    },

    QueryInterface: function(aIID) {
      if (SpecialPowers.wrap(aIID).equals(SpecialPowers.Ci.nsISupports) ||
          SpecialPowers.wrap(aIID).equals(SpecialPowers.Ci.nsIAlertsService) ||
          SpecialPowers.wrap(aIID).equals(SpecialPowers.Ci.nsIAppNotificationService)) {
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

  // MockServices API
  return {
    register: function () {
      registrar.registerFactory(MOCK_ALERTS_CID, "alerts service",
          ALERTS_SERVICE_CONTRACT_ID,
          mockAlertsService);

      registrar.registerFactory(MOCK_SYSTEM_ALERTS_CID, "system alerts service",
          SYSTEM_ALERTS_SERVICE_CONTRACT_ID,
          mockAlertsService);
    },

    unregister: function () {
      registrar.unregisterFactory(MOCK_ALERTS_CID, mockAlertsService);
      registrar.unregisterFactory(MOCK_SYSTEM_ALERTS_CID, mockAlertsService);
    },

    activeAlertNotifications: activeAlertNotifications,
    activeAppNotifications: activeAppNotifications,
  };
})();
