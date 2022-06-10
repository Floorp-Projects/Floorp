/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ASRouterParentProcessMessageHandler"];

const { ASRouterPreferences } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterPreferences.jsm"
);

const { MESSAGE_TYPE_HASH: msg } = ChromeUtils.import(
  "resource://activity-stream/common/ActorConstants.jsm"
);

class ASRouterParentProcessMessageHandler {
  constructor({
    router,
    preferences,
    specialMessageActions,
    queryCache,
    sendTelemetry,
  }) {
    this._router = router;
    this._preferences = preferences;
    this._specialMessageActions = specialMessageActions;
    this._queryCache = queryCache;
    this.handleTelemetry = sendTelemetry;
    this.handleMessage = this.handleMessage.bind(this);
    this.handleCFRAction = this.handleCFRAction.bind(this);
  }

  handleCFRAction({ type, data }, browser) {
    switch (type) {
      case msg.INFOBAR_TELEMETRY:
      case msg.TOOLBAR_BADGE_TELEMETRY:
      case msg.TOOLBAR_PANEL_TELEMETRY:
      case msg.MOMENTS_PAGE_TELEMETRY:
      case msg.DOORHANGER_TELEMETRY:
      case msg.SPOTLIGHT_TELEMETRY: {
        return this.handleTelemetry({ type, data });
      }
      default: {
        return this.handleMessage(type, data, { browser });
      }
    }
  }

  handleMessage(name, data, { id: tabId, browser } = { browser: null }) {
    switch (name) {
      case msg.AS_ROUTER_TELEMETRY_USER_EVENT:
        return this.handleTelemetry({
          type: msg.AS_ROUTER_TELEMETRY_USER_EVENT,
          data,
        });
      case msg.BLOCK_MESSAGE_BY_ID: {
        ASRouterPreferences.console.debug(
          "handleMesssage(): about to block, data = ",
          data
        );
        ASRouterPreferences.console.trace();

        // Block the message but don't dismiss it in case the action taken has
        // another state that needs to be visible
        return this._router
          .blockMessageById(data.id)
          .then(() => !data.preventDismiss);
      }
      case msg.USER_ACTION: {
        // This is to support ReturnToAMO
        if (data.type === "INSTALL_ADDON_FROM_URL") {
          this._router._updateOnboardingState();
        }
        return this._specialMessageActions.handleAction(data, browser);
      }
      case msg.IMPRESSION: {
        return this._router.addImpression(data);
      }
      case msg.TRIGGER: {
        return this._router.sendTriggerMessage({
          ...(data && data.trigger),
          tabId,
          browser,
        });
      }
      case msg.PBNEWTAB_MESSAGE_REQUEST: {
        return this._router.sendPBNewTabMessage({
          ...data,
          tabId,
          browser,
        });
      }
      case msg.NEWTAB_MESSAGE_REQUEST: {
        return this._router.sendNewTabMessage({
          ...data,
          tabId,
          browser,
        });
      }

      // ADMIN Messages
      case msg.ADMIN_CONNECT_STATE: {
        if (data && data.endpoint) {
          return this._router
            .addPreviewEndpoint(data.endpoint.url)
            .then(() => this._router.loadMessagesFromAllProviders());
        }
        return this._router.updateTargetingParameters();
      }
      case msg.UNBLOCK_MESSAGE_BY_ID: {
        return this._router.unblockMessageById(data.id);
      }
      case msg.UNBLOCK_ALL: {
        return this._router.unblockAll();
      }
      case msg.BLOCK_BUNDLE: {
        return this._router.blockMessageById(data.bundle.map(b => b.id));
      }
      case msg.UNBLOCK_BUNDLE: {
        return this._router.setState(state => {
          const messageBlockList = [...state.messageBlockList];
          for (let message of data.bundle) {
            messageBlockList.splice(messageBlockList.indexOf(message.id), 1);
          }
          this._router._storage.set("messageBlockList", messageBlockList);
          return { messageBlockList };
        });
      }
      case msg.DISABLE_PROVIDER: {
        this._preferences.enableOrDisableProvider(data, false);
        return Promise.resolve();
      }
      case msg.ENABLE_PROVIDER: {
        this._preferences.enableOrDisableProvider(data, true);
        return Promise.resolve();
      }
      case msg.EVALUATE_JEXL_EXPRESSION: {
        return this._router.evaluateExpression(data);
      }
      case msg.EXPIRE_QUERY_CACHE: {
        this._queryCache.expireAll();
        return Promise.resolve();
      }
      case msg.FORCE_ATTRIBUTION: {
        return this._router.forceAttribution(data);
      }
      case msg.FORCE_WHATSNEW_PANEL: {
        return this._router.forceWNPanel(browser);
      }
      case msg.CLOSE_WHATSNEW_PANEL: {
        return this._router.closeWNPanel(browser);
      }
      case msg.MODIFY_MESSAGE_JSON: {
        return this._router.routeCFRMessage(data.content, browser, data, true);
      }
      case msg.OVERRIDE_MESSAGE: {
        return this._router.setMessageById(data, true, browser);
      }
      case msg.RESET_PROVIDER_PREF: {
        this._preferences.resetProviderPref();
        return Promise.resolve();
      }
      case msg.SET_PROVIDER_USER_PREF: {
        this._preferences.setUserPreference(data.id, data.value);
        return Promise.resolve();
      }
      case msg.RESET_GROUPS_STATE: {
        return this._router
          .resetGroupsState(data)
          .then(() => this._router.loadMessagesFromAllProviders());
      }
      default: {
        return Promise.reject(new Error(`Unknown message received: ${name}`));
      }
    }
  }
}
