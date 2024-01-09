/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Contains elements of the Content Analysis UI, which are integrated into
 * various browser behaviors (uploading, downloading, printing, etc) that
 * require content analysis to be done.
 * The content analysis itself is done by the clients of this script, who
 * use nsIContentAnalysis to talk to the external CA system.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gContentAnalysis",
  "@mozilla.org/contentanalysis;1",
  Ci.nsIContentAnalysis
);

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "silentNotifications",
  "browser.contentanalysis.silent_notifications",
  false
);

/**
 * A class that groups browsing contexts by their top-level one.
 * This is necessary because if there may be a subframe that
 * is showing a "DLP request busy" dialog when another subframe
 * (other the outer frame) wants to show one. This class makes it
 * convenient to find if another frame with the same top browsing
 * context is currently showing a dialog, and also to find if there
 * are any pending dialogs to show when one closes.
 */
class MapByTopBrowsingContext {
  #map;
  constructor() {
    this.#map = new Map();
  }
  /**
   * Gets any existing data associated with the browsing context
   *
   * @param {BrowsingContext} aBrowsingContext the browsing context to search for
   * @returns {object | undefined} the existing data, or `undefined` if there is none
   */
  getEntry(aBrowsingContext) {
    const topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      return undefined;
    }
    return topEntry.get(aBrowsingContext);
  }
  /**
   * Returns whether the browsing context has any data associated with it
   *
   * @param {BrowsingContext} aBrowsingContext the browsing context to search for
   * @returns {boolean} Whether the browsing context has any associated data
   */
  hasEntry(aBrowsingContext) {
    const topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      return false;
    }
    return topEntry.has(aBrowsingContext);
  }
  /**
   * Whether the tab containing the browsing context has a dialog
   * currently showing
   *
   * @param {BrowsingContext} aBrowsingContext the browsing context to search for
   * @returns {boolean} whether the tab has a dialog currently showing
   */
  hasEntryDisplayingNotification(aBrowsingContext) {
    const topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      return false;
    }
    for (const otherEntry in topEntry.values()) {
      if (otherEntry.notification?.dialogBrowsingContext) {
        return true;
      }
    }
    return false;
  }
  /**
   * Gets another browsing context in the same tab that has pending "DLP busy" dialog
   * info to show, if any.
   *
   * @param {BrowsingContext} aBrowsingContext the browsing context to search for
   * @returns {BrowsingContext} Another browsing context in the same tab that has pending "DLP busy" dialog info, or `undefined` if there aren't any.
   */
  getBrowsingContextWithPendingNotification(aBrowsingContext) {
    const topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      return undefined;
    }
    if (aBrowsingContext.top.isDiscarded) {
      // The top-level tab has already been closed, so remove
      // the top-level entry and return there are no pending dialogs.
      this.#map.delete(aBrowsingContext.top);
      return undefined;
    }
    for (const otherContext in topEntry.keys()) {
      if (
        topEntry.get(otherContext).notification?.dialogBrowsingContextArgs &&
        otherContext !== aBrowsingContext
      ) {
        return otherContext;
      }
    }
    return undefined;
  }
  /**
   * Deletes the entry for the browsing context, if any
   *
   * @param {BrowsingContext} aBrowsingContext the browsing context to delete
   * @returns {boolean} Whether an entry was deleted or not
   */
  deleteEntry(aBrowsingContext) {
    const topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      return false;
    }
    const toReturn = topEntry.delete(aBrowsingContext);
    if (!topEntry.size || aBrowsingContext.top.isDiscarded) {
      // Either the inner Map is now empty, or the whole tab
      // has been closed. Either way, remove the top-level entry.
      this.#map.delete(aBrowsingContext.top);
    }
    return toReturn;
  }
  /**
   * Sets the associated data for the browsing context
   *
   * @param {BrowsingContext} aBrowsingContext the browsing context to set the data for
   * @param {object} aValue the data to associated with the browsing context
   * @returns {MapByTopBrowsingContext} this
   */
  setEntry(aBrowsingContext, aValue) {
    let topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      topEntry = new Map();
      this.#map.set(aBrowsingContext.top, topEntry);
    }
    topEntry.set(aBrowsingContext, aValue);
    return this;
  }
}

export const ContentAnalysis = {
  _SHOW_NOTIFICATIONS: true,

  _SHOW_DIALOGS: false,

  _SLOW_DLP_NOTIFICATION_BLOCKING_TIMEOUT_MS: 250,

  _SLOW_DLP_NOTIFICATION_NONBLOCKING_TIMEOUT_MS: 3 * 1000,

  _RESULT_NOTIFICATION_TIMEOUT_MS: 5 * 60 * 1000, // 5 min

  _RESULT_NOTIFICATION_FAST_TIMEOUT_MS: 60 * 1000, // 1 min

  isInitialized: false,

  dlpBusyViewsByTopBrowsingContext: new MapByTopBrowsingContext(),

  requestTokenToRequestInfo: new Map(),

  /**
   * Registers for various messages/events that will indicate the
   * need for communicating something to the user.
   */
  initialize() {
    if (!this.isInitialized) {
      this.isInitialized = true;
      this.initializeDownloadCA();

      ChromeUtils.defineLazyGetter(this, "l10n", function () {
        return new Localization(
          ["toolkit/contentanalysis/contentanalysis.ftl"],
          true
        );
      });
    }
  },

  async uninitialize() {
    if (this.isInitialized) {
      this.isInitialized = false;
      this.requestTokenToRequestInfo.clear();
    }
  },

  /**
   * Register UI for file download CA events.
   */
  async initializeDownloadCA() {
    Services.obs.addObserver(this, "dlp-request-made");
    Services.obs.addObserver(this, "dlp-response");
    Services.obs.addObserver(this, "quit-application");
  },

  // nsIObserver
  async observe(aSubj, aTopic, aData) {
    switch (aTopic) {
      case "quit-application": {
        this.uninitialize();
        break;
      }
      case "dlp-request-made":
        {
          const request = aSubj.QueryInterface(Ci.nsIContentAnalysisRequest);
          if (!request) {
            console.error(
              "Showing in-browser Content Analysis notification but no request was passed"
            );
            return;
          }
          const operation = request.analysisType;
          // For operations that block browser interaction, show the "slow content analysis"
          // dialog faster
          let slowTimeoutMs = this._shouldShowBlockingNotification(operation)
            ? this._SLOW_DLP_NOTIFICATION_BLOCKING_TIMEOUT_MS
            : this._SLOW_DLP_NOTIFICATION_NONBLOCKING_TIMEOUT_MS;
          let browsingContext = request.windowGlobalParent?.browsingContext;
          if (!browsingContext) {
            throw new Error(
              "Got dlp-request-made message but couldn't find a browsingContext!"
            );
          }

          // Start timer that, when it expires,
          // presents a "slow CA check" message.
          // Note that there should only be one DLP request
          // at a time per browsingContext (since we block the UI and
          // the content process waits synchronously for the result).
          if (this.dlpBusyViewsByTopBrowsingContext.hasEntry(browsingContext)) {
            throw new Error(
              "Got dlp-request-made message for a browsingContext that already has a busy view!"
            );
          }
          let resourceNameOrL10NId =
            this._getResourceNameOrL10NIdFromRequest(request);
          this.requestTokenToRequestInfo.set(request.requestToken, {
            browsingContext,
            resourceNameOrL10NId,
          });
          this.dlpBusyViewsByTopBrowsingContext.setEntry(browsingContext, {
            timer: lazy.setTimeout(() => {
              this.dlpBusyViewsByTopBrowsingContext.setEntry(browsingContext, {
                notification: this._showSlowCAMessage(
                  operation,
                  request,
                  resourceNameOrL10NId,
                  browsingContext
                ),
              });
            }, slowTimeoutMs),
          });
        }
        break;
      case "dlp-response":
        const request = aSubj.QueryInterface(Ci.nsIContentAnalysisResponse);
        // Cancels timer or slow message UI,
        // if present, and possibly presents the CA verdict.
        if (!request) {
          throw new Error("Got dlp-response message but no request was passed");
        }

        let windowAndResourceNameOrL10NId = this.requestTokenToRequestInfo.get(
          request.requestToken
        );
        if (!windowAndResourceNameOrL10NId) {
          // Perhaps this was cancelled just before the response came in from the
          // DLP agent.
          console.warn(
            `Got dlp-response message with unknown token ${request.requestToken}`
          );
          return;
        }
        this.requestTokenToRequestInfo.delete(request.requestToken);
        let dlpBusyView = this.dlpBusyViewsByTopBrowsingContext.getEntry(
          windowAndResourceNameOrL10NId.browsingContext
        );
        if (dlpBusyView) {
          this._disconnectFromView(dlpBusyView);
          this.dlpBusyViewsByTopBrowsingContext.deleteEntry(
            windowAndResourceNameOrL10NId.browsingContext
          );
        }
        const responseResult =
          request?.action ?? Ci.nsIContentAnalysisResponse.eUnspecified;
        await this._showCAResult(
          windowAndResourceNameOrL10NId.resourceNameOrL10NId,
          windowAndResourceNameOrL10NId.browsingContext,
          request.requestToken,
          responseResult
        );
        this._showAnotherPendingDialog(
          windowAndResourceNameOrL10NId.browsingContext
        );
        break;
    }
  },

  _showAnotherPendingDialog(aBrowsingContext) {
    const otherBrowsingContext =
      this.dlpBusyViewsByTopBrowsingContext.getBrowsingContextWithPendingNotification(
        aBrowsingContext
      );
    if (otherBrowsingContext) {
      const args =
        this.dlpBusyViewsByTopBrowsingContext.getEntry(otherBrowsingContext);
      this.dlpBusyViewsByTopBrowsingContext.setEntry(otherBrowsingContext, {
        notification: this._showSlowCABlockingMessage(
          otherBrowsingContext,
          args.requestToken,
          args.resourceNameOrL10NId
        ),
      });
    }
  },

  _disconnectFromView(caView) {
    if (!caView) {
      return;
    }
    if (caView.timer) {
      lazy.clearTimeout(caView.timer);
    } else if (caView.notification) {
      if (caView.notification.close) {
        // native notification
        caView.notification.close();
      } else if (caView.notification.dialogBrowsingContext) {
        // in-browser notification
        let browser =
          caView.notification.dialogBrowsingContext.top.embedderElement;
        // browser will be null if the tab was closed
        let win = browser?.ownerGlobal;
        if (win) {
          let dialogBox = win.gBrowser.getTabDialogBox(browser);
          // Don't close any content-modal dialogs, because we could be doing
          // content analysis on something like a prompt() call.
          dialogBox.getTabDialogManager().abortDialogs();
        }
      } else {
        console.error(
          "Unexpected content analysis notification - can't close it!"
        );
      }
    }
  },

  _showMessage(aMessage, aBrowsingContext, aTimeout = 0) {
    if (this._SHOW_DIALOGS) {
      Services.prompt.asyncAlert(
        aBrowsingContext,
        Ci.nsIPrompt.MODAL_TYPE_WINDOW,
        this.l10n.formatValueSync("contentanalysis-alert-title"),
        aMessage
      );
    }

    if (this._SHOW_NOTIFICATIONS) {
      const notification = new aBrowsingContext.topChromeWindow.Notification(
        this.l10n.formatValueSync("contentanalysis-notification-title"),
        {
          body: aMessage,
          silent: lazy.silentNotifications,
        }
      );

      if (aTimeout != 0) {
        lazy.setTimeout(() => {
          notification.close();
        }, aTimeout);
      }
      return notification;
    }

    return null;
  },

  _shouldShowBlockingNotification(aOperation) {
    return !(
      aOperation == Ci.nsIContentAnalysisRequest.eFileDownloaded ||
      aOperation == Ci.nsIContentAnalysisRequest.ePrint
    );
  },

  // This function also transforms the nameOrL10NId so we won't have to
  // look it up again.
  _getResourceNameFromNameOrL10NId(nameOrL10NId) {
    if (nameOrL10NId.name) {
      return nameOrL10NId.name;
    }
    nameOrL10NId.name = this.l10n.formatValueSync(nameOrL10NId.l10nId);
    return nameOrL10NId.name;
  },

  _getResourceNameOrL10NIdFromRequest(aRequest) {
    if (
      aRequest.operationTypeForDisplay ==
      Ci.nsIContentAnalysisRequest.eCustomDisplayString
    ) {
      return { name: aRequest.operationDisplayString };
    }
    let l10nId;
    switch (aRequest.operationTypeForDisplay) {
      case Ci.nsIContentAnalysisRequest.eClipboard:
        l10nId = "contentanalysis-operationtype-clipboard";
        break;
      case Ci.nsIContentAnalysisRequest.eDroppedText:
        l10nId = "contentanalysis-operationtype-dropped-text";
        break;
    }
    if (!l10nId) {
      console.error(
        "Unknown operationTypeForDisplay: " + aRequest.operationTypeForDisplay
      );
      return { name: "" };
    }
    return { l10nId };
  },

  /**
   * Show a message to the user to indicate that a CA request is taking
   * a long time.
   */
  _showSlowCAMessage(
    aOperation,
    aRequest,
    aResourceNameOrL10NId,
    aBrowsingContext
  ) {
    if (!this._shouldShowBlockingNotification(aOperation)) {
      return this._showMessage(
        this.l10n.formatValueSync("contentanalysis-slow-agent-notification", {
          content: this._getResourceNameFromNameOrL10NId(aResourceNameOrL10NId),
        }),
        aBrowsingContext
      );
    }

    if (!aRequest) {
      throw new Error(
        "Showing in-browser Content Analysis notification but no request was passed"
      );
    }

    if (
      this.dlpBusyViewsByTopBrowsingContext.hasEntryDisplayingNotification(
        aBrowsingContext
      )
    ) {
      // This tab already has a frame displaying a "DLP in progress" message, so we can't
      // show another one right now. Record the arguments we will need to show another
      // "DLP in progress" message when the existing message goes away.
      return {
        requestToken: aRequest.requestToken,
        dialogBrowsingContextArgs: {
          resourceNameOrL10NId: aResourceNameOrL10NId,
        },
      };
    }

    return this._showSlowCABlockingMessage(
      aBrowsingContext,
      aRequest.requestToken,
      aResourceNameOrL10NId
    );
  },

  _showSlowCABlockingMessage(
    aBrowsingContext,
    aRequestToken,
    aResourceNameOrL10NId
  ) {
    let promise = Services.prompt.asyncConfirmEx(
      aBrowsingContext,
      Ci.nsIPromptService.MODAL_TYPE_TAB,
      this.l10n.formatValueSync("contentanalysis-slow-agent-dialog-title"),
      this.l10n.formatValueSync("contentanalysis-slow-agent-dialog-body", {
        content: this._getResourceNameFromNameOrL10NId(aResourceNameOrL10NId),
      }),
      Ci.nsIPromptService.BUTTON_POS_0 *
        Ci.nsIPromptService.BUTTON_TITLE_CANCEL +
        Ci.nsIPromptService.SHOW_SPINNER,
      null,
      null,
      null,
      null,
      false
    );
    promise
      .catch(() => {
        // need a catch clause to avoid an unhandled JS exception
        // when we programmatically close the dialog.
        // Since this only happens when we are programmatically closing
        // the dialog, no need to log the exception.
      })
      .finally(() => {
        // This is also be called if the tab/window is closed while a request is in progress,
        // in which case we need to cancel the request.
        if (this.requestTokenToRequestInfo.delete(aRequestToken)) {
          lazy.gContentAnalysis.cancelContentAnalysisRequest(aRequestToken);
          let dlpBusyView =
            this.dlpBusyViewsByTopBrowsingContext.getEntry(aBrowsingContext);
          if (dlpBusyView) {
            this._disconnectFromView(dlpBusyView);
            this.dlpBusyViewsByTopBrowsingContext.deleteEntry(aBrowsingContext);
          }
        }
      });
    return {
      requestToken: aRequestToken,
      dialogBrowsingContext: aBrowsingContext,
    };
  },

  /**
   * Show a message to the user to indicate the result of a CA request.
   *
   * @returns {object} a notification object (if shown)
   */
  async _showCAResult(
    aResourceNameOrL10NId,
    aBrowsingContext,
    aRequestToken,
    aCAResult
  ) {
    let message = null;
    let timeoutMs = 0;

    switch (aCAResult) {
      case Ci.nsIContentAnalysisResponse.eAllow:
        // We don't need to show anything
        return null;
      case Ci.nsIContentAnalysisResponse.eReportOnly:
        message = await this.l10n.formatValue(
          "contentanalysis-genericresponse-message",
          {
            content: this._getResourceNameFromNameOrL10NId(
              aResourceNameOrL10NId
            ),
            response: "REPORT_ONLY",
          }
        );
        timeoutMs = this._RESULT_NOTIFICATION_FAST_TIMEOUT_MS;
        break;
      case Ci.nsIContentAnalysisResponse.eWarn:
        const result = await Services.prompt.asyncConfirmEx(
          aBrowsingContext,
          Ci.nsIPromptService.MODAL_TYPE_TAB,
          await this.l10n.formatValue("contentanalysis-warndialogtitle"),
          await this.l10n.formatValue("contentanalysis-warndialogtext", {
            content: this._getResourceNameFromNameOrL10NId(
              aResourceNameOrL10NId
            ),
          }),
          Ci.nsIPromptService.BUTTON_POS_0 *
            Ci.nsIPromptService.BUTTON_TITLE_IS_STRING +
            Ci.nsIPromptService.BUTTON_POS_1 *
              Ci.nsIPromptService.BUTTON_TITLE_IS_STRING +
            Ci.nsIPromptService.BUTTON_POS_1_DEFAULT,
          await this.l10n.formatValue(
            "contentanalysis-warndialog-response-allow"
          ),
          await this.l10n.formatValue(
            "contentanalysis-warndialog-response-deny"
          ),
          null,
          null,
          {}
        );
        const allow = result.get("buttonNumClicked") === 0;
        lazy.gContentAnalysis.respondToWarnDialog(aRequestToken, allow);
        return null;
      case Ci.nsIContentAnalysisResponse.eBlock:
        message = await this.l10n.formatValue("contentanalysis-block-message", {
          content: this._getResourceNameFromNameOrL10NId(aResourceNameOrL10NId),
        });
        timeoutMs = this._RESULT_NOTIFICATION_TIMEOUT_MS;
        break;
      case Ci.nsIContentAnalysisResponse.eUnspecified:
        message = await this.l10n.formatValue("contentanalysis-error-message", {
          content: this._getResourceNameFromNameOrL10NId(aResourceNameOrL10NId),
        });
        timeoutMs = this._RESULT_NOTIFICATION_TIMEOUT_MS;
        break;
      default:
        throw new Error("Unexpected CA result value: " + aCAResult);
    }

    if (!message) {
      console.error(
        "_showCAResult did not get a message populated for result value " +
          aCAResult
      );
      return null;
    }

    return this._showMessage(message, aBrowsingContext, timeoutMs);
  },
};
