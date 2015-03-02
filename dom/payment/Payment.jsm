/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = [];

const PAYMENT_IPC_MSG_NAMES = ["Payment:Pay",
                               "Payment:Success",
                               "Payment:Failed"];

const PREF_PAYMENTPROVIDERS_BRANCH = "dom.payment.provider.";
const PREF_PAYMENT_BRANCH = "dom.payment.";
const PREF_DEBUG = "dom.payment.debug";

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyServiceGetter(this, "prefService",
                                   "@mozilla.org/preferences-service;1",
                                   "nsIPrefService");

let PaymentManager =  {
  init: function init() {
    // Payment providers data are stored as a preference.
    this.registeredProviders = null;

    this.messageManagers = {};

    // The dom.payment.skipHTTPSCheck pref is supposed to be used only during
    // development process. This preference should not be active for a
    // production build.
    let paymentPrefs = prefService.getBranch(PREF_PAYMENT_BRANCH);
    this.checkHttps = true;
    try {
      if (paymentPrefs.getPrefType("skipHTTPSCheck")) {
        this.checkHttps = !paymentPrefs.getBoolPref("skipHTTPSCheck");
      }
    } catch(e) {}

    for each (let msgname in PAYMENT_IPC_MSG_NAMES) {
      ppmm.addMessageListener(msgname, this);
    }

    Services.obs.addObserver(this, "xpcom-shutdown", false);

    try {
      this._debug =
        Services.prefs.getPrefType(PREF_DEBUG) == Ci.nsIPrefBranch.PREF_BOOL
        && Services.prefs.getBoolPref(PREF_DEBUG);
    } catch(e) {
      this._debug = false;
    }
  },

  /**
   * Process a message from the content process.
   */
  receiveMessage: function receiveMessage(aMessage) {
    let name = aMessage.name;
    let msg = aMessage.json;
    if (this._debug) {
      this.LOG("Received '" + name + "' message from content process");
    }

    switch (name) {
      case "Payment:Pay": {
        // First of all, we register the payment providers.
        if (!this.registeredProviders) {
          this.registeredProviders = {};
          this.registerPaymentProviders();
        }

        // We save the message target message manager so we can later dispatch
        // back messages without broadcasting to all child processes.
        let requestId = msg.requestId;
        this.messageManagers[requestId] = aMessage.target;

        // We check the jwt type and look for a match within the
        // registered payment providers to get the correct payment request
        // information.
        let paymentRequests = [];
        let jwtTypes = [];
        for (let i in msg.jwts) {
          let pr = this.getPaymentRequestInfo(requestId, msg.jwts[i]);
          if (!pr) {
            continue;
          }
          // We consider jwt type repetition an error.
          if (jwtTypes[pr.type]) {
            this.paymentFailed(requestId,
                               "PAY_REQUEST_ERROR_DUPLICATED_JWT_TYPE");
            return;
          }
          jwtTypes[pr.type] = true;
          paymentRequests.push(pr);
        }

        if (!paymentRequests.length) {
          this.paymentFailed(requestId,
                             "PAY_REQUEST_ERROR_NO_VALID_REQUEST_FOUND");
          return;
        }

        // After getting the list of valid payment requests, we ask the user
        // for confirmation before sending any request to any payment provider.
        // If there is more than one choice, we also let the user select the one
        // that he prefers.
        let glue = Cc["@mozilla.org/payment/ui-glue;1"]
                   .createInstance(Ci.nsIPaymentUIGlue);
        if (!glue) {
          if (this._debug) {
            this.LOG("Could not create nsIPaymentUIGlue instance");
          }
          this.paymentFailed(requestId,
                             "INTERNAL_ERROR_CREATE_PAYMENT_GLUE_FAILED");
          return;
        }

        let confirmPaymentSuccessCb = function successCb(aRequestId,
                                                         aResult) {
          // Get the appropriate payment provider data based on user's choice.
          let selectedProvider = this.registeredProviders[aResult];
          if (!selectedProvider || !selectedProvider.uri) {
            if (this._debug) {
              this.LOG("Could not retrieve a valid provider based on user's " +
                        "selection");
            }
            this.paymentFailed(aRequestId,
                               "INTERNAL_ERROR_NO_VALID_SELECTED_PROVIDER");
            return;
          }

          let jwt;
          for (let i in paymentRequests) {
            if (paymentRequests[i].type == aResult) {
              jwt = paymentRequests[i].jwt;
              break;
            }
          }
          if (!jwt) {
            if (this._debug) {
              this.LOG("The selected request has no JWT information " +
                        "associated");
            }
            this.paymentFailed(aRequestId,
                               "INTERNAL_ERROR_NO_JWT_ASSOCIATED_TO_REQUEST");
            return;
          }

          this.showPaymentFlow(aRequestId, selectedProvider, jwt);
        };

        let confirmPaymentErrorCb = this.paymentFailed;

        glue.confirmPaymentRequest(requestId,
                                   paymentRequests,
                                   confirmPaymentSuccessCb.bind(this),
                                   confirmPaymentErrorCb.bind(this));
        break;
      }
      case "Payment:Success":
      case "Payment:Failed": {
        let mm = this.messageManagers[msg.requestId];
        mm.sendAsyncMessage(name, {
          requestId: msg.requestId,
          result: msg.result,
          errorMsg: msg.errorMsg
        });
        break;
      }
    }
  },

  /**
   * Helper function to register payment providers stored as preferences.
   */
  registerPaymentProviders: function registerPaymentProviders() {
    let paymentProviders = prefService
                           .getBranch(PREF_PAYMENTPROVIDERS_BRANCH)
                           .getChildList("");

    // First get the numbers of the providers by getting all ###.uri prefs.
    let nums = [];
    for (let i in paymentProviders) {
      let match = /^(\d+)\.uri$/.exec(paymentProviders[i]);
      if (!match) {
        continue;
      } else {
        nums.push(match[1]);
      }
    }

#ifdef MOZ_B2G
    let appsService = Cc["@mozilla.org/AppsService;1"]
                        .getService(Ci.nsIAppsService);
    let systemAppId = Ci.nsIScriptSecurityManager.NO_APP_ID;

    try {
      let manifestURL = Services.prefs.getCharPref("b2g.system_manifest_url");
      systemAppId = appsService.getAppLocalIdByManifestURL(manifestURL);
      this.LOG("System app id=" + systemAppId);
    } catch(e) {}
#endif

    // Now register the payment providers.
    for (let i in nums) {
      let branch = prefService
                   .getBranch(PREF_PAYMENTPROVIDERS_BRANCH + nums[i] + ".");
      let vals = branch.getChildList("");
      if (vals.length == 0) {
        return;
      }
      try {
        let type = branch.getCharPref("type");
        if (type in this.registeredProviders) {
          continue;
        }
        let provider = this.registeredProviders[type] = {
          name: branch.getCharPref("name"),
          uri: branch.getCharPref("uri"),
          description: branch.getCharPref("description"),
          requestMethod: branch.getCharPref("requestMethod")
        };

#ifdef MOZ_B2G
        // Let this payment provider access the firefox-accounts API when
        // it's loaded in the trusted UI.
        if (systemAppId != Ci.nsIScriptSecurityManager.NO_APP_ID) {
          this.LOG("Granting firefox-accounts permission to " + provider.uri);
          let uri = Services.io.newURI(provider.uri, null, null);
          let principal = Services.scriptSecurityManager
                            .getAppCodebasePrincipal(uri, systemAppId, true);

          Services.perms.addFromPrincipal(principal, "firefox-accounts",
                                          Ci.nsIPermissionManager.ALLOW_ACTION,
                                          Ci.nsIPermissionManager.EXPIRE_SESSION);
        }
#endif

        if (this._debug) {
          this.LOG("Registered Payment Providers: " +
                    JSON.stringify(this.registeredProviders[type]));
        }
      } catch (ex) {
        if (this._debug) {
          this.LOG("An error ocurred registering a payment provider. " + ex);
        }
      }
    }
  },

  /**
   * Helper for sending a Payment:Failed message to the parent process.
   */
  paymentFailed: function paymentFailed(aRequestId, aErrorMsg) {
    let mm = this.messageManagers[aRequestId];
    mm.sendAsyncMessage("Payment:Failed", {
      requestId: aRequestId,
      errorMsg: aErrorMsg
    });
  },

  /**
   * Helper function to get the payment request info according to the jwt
   * type. Payment provider's data is stored as a preference.
   */
  getPaymentRequestInfo: function getPaymentRequestInfo(aRequestId, aJwt) {
    if (!aJwt) {
      this.paymentFailed(aRequestId, "INTERNAL_ERROR_CALL_WITH_MISSING_JWT");
      return true;
    }

    // First thing, we check that the jwt type is an allowed type and has a
    // payment provider flow information associated.

    // A jwt string consists in three parts separated by period ('.'): header,
    // payload and signature.
    let segments = aJwt.split('.');
    if (segments.length !== 3) {
      if (this._debug) {
        this.LOG("Error getting payment provider's uri. " +
                  "Not enough or too many segments");
      }
      this.paymentFailed(aRequestId,
                         "PAY_REQUEST_ERROR_WRONG_SEGMENTS_COUNT");
      return true;
    }

    let payloadObject;
    try {
      // We only care about the payload segment, which contains the jwt type
      // that should match with any of the stored payment provider's data and
      // the payment request information to be shown to the user.
      // Before decoding the JWT string we need to normalize it to be compliant
      // with RFC 4648.
      segments[1] = segments[1].replace(/-/g, "+").replace(/_/g, "/");
      let payload = atob(segments[1]);
      if (this._debug) {
        this.LOG("Payload " + payload);
      }
      if (!payload.length) {
        this.paymentFailed(aRequestId, "PAY_REQUEST_ERROR_EMPTY_PAYLOAD");
        return true;
      }
      payloadObject = JSON.parse(payload);
      if (!payloadObject) {
        this.paymentFailed(aRequestId,
                           "PAY_REQUEST_ERROR_ERROR_PARSING_JWT_PAYLOAD");
        return true;
      }
    } catch (e) {
      this.paymentFailed(aRequestId,
                         "PAY_REQUEST_ERROR_ERROR_DECODING_JWT");
      return true;
    }

    if (!payloadObject.typ) {
      this.paymentFailed(aRequestId,
                         "PAY_REQUEST_ERROR_NO_TYP_PARAMETER");
      return true;
    }

    if (!payloadObject.request) {
      this.paymentFailed(aRequestId,
                         "PAY_REQUEST_ERROR_NO_REQUEST_PARAMETER");
      return true;
    }

    // Once we got the jwt 'typ' value we look for a match within the payment
    // providers stored preferences. If the jwt 'typ' is not recognized as one
    // of the allowed values for registered payment providers, we skip the jwt
    // validation but we don't fire any error. This way developers might have
    // a default set of well formed JWTs that might be used in different B2G
    // devices with a different set of allowed payment providers.
    let provider = this.registeredProviders[payloadObject.typ];
    if (!provider) {
      if (this._debug) {
        this.LOG("Not registered payment provider for jwt type: " +
                  payloadObject.typ);
      }
      return false;
    }

    if (!provider.uri || !provider.name) {
      this.paymentFailed(aRequestId,
                         "INTERNAL_ERROR_WRONG_REGISTERED_PAY_PROVIDER");
      return true;
    }

    // We only allow https for payment providers uris.
    if (this.checkHttps && !/^https/.exec(provider.uri.toLowerCase())) {
      // We should never get this far.
      if (this._debug) {
        this.LOG("Payment provider uris must be https: " + provider.uri);
      }
      this.paymentFailed(aRequestId,
                         "INTERNAL_ERROR_NON_HTTPS_PROVIDER_URI");
      return true;
    }

    let pldRequest = payloadObject.request;
    return { jwt: aJwt, type: payloadObject.typ, providerName: provider.name };
  },

  showPaymentFlow: function showPaymentFlow(aRequestId,
                                            aPaymentProvider,
                                            aJwt) {
    let paymentFlowInfo = Cc["@mozilla.org/payment/flow-info;1"]
                          .createInstance(Ci.nsIPaymentFlowInfo);
    paymentFlowInfo.uri = aPaymentProvider.uri;
    paymentFlowInfo.requestMethod = aPaymentProvider.requestMethod;
    paymentFlowInfo.jwt = aJwt;
    paymentFlowInfo.name = aPaymentProvider.name;
    paymentFlowInfo.description = aPaymentProvider.description;

    let glue = Cc["@mozilla.org/payment/ui-glue;1"]
               .createInstance(Ci.nsIPaymentUIGlue);
    if (!glue) {
      if (this._debug) {
        this.LOG("Could not create nsIPaymentUIGlue instance");
      }
      this.paymentFailed(aRequestId,
                         "INTERNAL_ERROR_CREATE_PAYMENT_GLUE_FAILED");
      return false;
    }
    glue.showPaymentFlow(aRequestId,
                         paymentFlowInfo,
                         this.paymentFailed.bind(this));
  },

  // nsIObserver

  observe: function observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      for each (let msgname in PAYMENT_IPC_MSG_NAMES) {
        ppmm.removeMessageListener(msgname, this);
      }
      this.registeredProviders = null;
      this.messageManagers = null;

      Services.obs.removeObserver(this, "xpcom-shutdown");
    }
  },

  LOG: function LOG(s) {
    if (!this._debug) {
      return;
    }
    dump("-*- PaymentManager: " + s + "\n");
  }
};

PaymentManager.init();
