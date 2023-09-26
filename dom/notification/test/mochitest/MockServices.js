var MockServices = (function () {
  "use strict";

  const MOCK_ALERTS_CID = SpecialPowers.wrap(SpecialPowers.Components).ID(
    "{48068bc2-40ab-4904-8afd-4cdfb3a385f3}"
  );
  const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";

  const MOCK_SYSTEM_ALERTS_CID = SpecialPowers.wrap(
    SpecialPowers.Components
  ).ID("{e86d888c-e41b-4b78-9104-2f2742a532de}");
  const SYSTEM_ALERTS_SERVICE_CONTRACT_ID =
    "@mozilla.org/system-alerts-service;1";

  var registrar = SpecialPowers.wrap(
    SpecialPowers.Components
  ).manager.QueryInterface(SpecialPowers.Ci.nsIComponentRegistrar);

  var activeAlertNotifications = Object.create(null);

  var activeAppNotifications = Object.create(null);

  window.addEventListener("mock-notification-close-event", function (e) {
    for (var alertName in activeAlertNotifications) {
      var notif = activeAlertNotifications[alertName];
      if (notif.title === e.detail.title) {
        notif.listener.observe(null, "alertfinished", null);
        delete activeAlertNotifications[alertName];
        delete activeAppNotifications[alertName];
        return;
      }
    }
  });

  var mockAlertsService = {
    showPersistentNotification(persistentData, alert, alertListener) {
      this.showAlert(alert, alertListener);
    },

    showAlert(alert, alertListener) {
      var listener = SpecialPowers.wrap(alertListener);
      activeAlertNotifications[alert.name] = {
        listener,
        cookie: alert.cookie,
        title: alert.title,
      };

      // fake async alert show event
      if (listener) {
        setTimeout(function () {
          listener.observe(null, "alertshow", alert.cookie);
        }, 100);
        setTimeout(function () {
          listener.observe(null, "alertclickcallback", alert.cookie);
        }, 100);
      }
    },

    showAlertNotification(
      imageUrl,
      title,
      text,
      textClickable,
      cookie,
      alertListener,
      name
    ) {
      this.showAlert(
        {
          name,
          cookie,
          title,
        },
        alertListener
      );
    },

    closeAlert(name) {
      var alertNotification = activeAlertNotifications[name];
      if (alertNotification) {
        if (alertNotification.listener) {
          alertNotification.listener.observe(
            null,
            "alertfinished",
            alertNotification.cookie
          );
        }
        delete activeAlertNotifications[name];
      }

      var appNotification = activeAppNotifications[name];
      if (appNotification) {
        delete activeAppNotifications[name];
      }
    },

    QueryInterface(aIID) {
      if (
        SpecialPowers.wrap(aIID).equals(SpecialPowers.Ci.nsISupports) ||
        SpecialPowers.wrap(aIID).equals(SpecialPowers.Ci.nsIAlertsService)
      ) {
        return this;
      }
      throw SpecialPowers.Components.results.NS_ERROR_NO_INTERFACE;
    },

    createInstance(aIID) {
      return this.QueryInterface(aIID);
    },
  };
  mockAlertsService = SpecialPowers.wrapCallbackObject(mockAlertsService);

  // MockServices API
  return {
    register() {
      try {
        this.originalAlertsCID = registrar.contractIDToCID(
          ALERTS_SERVICE_CONTRACT_ID
        );
      } catch (ex) {
        this.originalAlertsCID = null;
      }
      try {
        this.originalSystemAlertsCID = registrar.contractIDToCID(
          SYSTEM_ALERTS_SERVICE_CONTRACT_ID
        );
      } catch (ex) {
        this.originalSystemAlertsCID = null;
      }

      registrar.registerFactory(
        MOCK_ALERTS_CID,
        "alerts service",
        ALERTS_SERVICE_CONTRACT_ID,
        mockAlertsService
      );

      registrar.registerFactory(
        MOCK_SYSTEM_ALERTS_CID,
        "system alerts service",
        SYSTEM_ALERTS_SERVICE_CONTRACT_ID,
        mockAlertsService
      );
    },

    unregister() {
      registrar.unregisterFactory(MOCK_ALERTS_CID, mockAlertsService);
      registrar.unregisterFactory(MOCK_SYSTEM_ALERTS_CID, mockAlertsService);

      // Passing `null` for the factory re-maps the contract ID to the
      // entry for its original CID.

      if (this.originalAlertsCID) {
        registrar.registerFactory(
          this.originalAlertsCID,
          "alerts service",
          ALERTS_SERVICE_CONTRACT_ID,
          null
        );
      }

      if (this.originalSystemAlertsCID) {
        registrar.registerFactory(
          this.originalSystemAlertsCID,
          "system alerts service",
          SYSTEM_ALERTS_SERVICE_CONTRACT_ID,
          null
        );
      }
    },

    activeAlertNotifications,

    activeAppNotifications,
  };
})();
