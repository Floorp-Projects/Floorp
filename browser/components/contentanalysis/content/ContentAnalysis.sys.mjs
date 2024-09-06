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
  PanelMultiView: "resource:///modules/PanelMultiView.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "silentNotifications",
  "browser.contentanalysis.silent_notifications",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "agentName",
  "browser.contentanalysis.agent_name",
  "A DLP agent"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "showBlockedResult",
  "browser.contentanalysis.show_blocked_result",
  true
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
    if (!aValue.request) {
      console.error(
        "MapByTopBrowsingContext.setEntry() called with a value without a request!"
      );
    }
    let topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      topEntry = new Map();
      this.#map.set(aBrowsingContext.top, topEntry);
    }
    topEntry.set(aBrowsingContext, aValue);
    return this;
  }

  getAllRequests() {
    let requests = [];
    this.#map.forEach(topEntry => {
      for (let entry of topEntry.values()) {
        requests.push(entry.request);
      }
    });
    return requests;
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
  initialize(doc) {
    if (!lazy.gContentAnalysis.isActive) {
      return;
    }
    if (!this.isInitialized) {
      this.isInitialized = true;
      this.initializeDownloadCA();

      ChromeUtils.defineLazyGetter(this, "l10n", function () {
        return new Localization(
          ["branding/brand.ftl", "toolkit/contentanalysis/contentanalysis.ftl"],
          true
        );
      });
    }

    // Do this even if initialized so the icon shows up on new windows, not just the
    // first one.
    doc.l10n.setAttributes(
      doc.getElementById("content-analysis-indicator"),
      "content-analysis-indicator-tooltip",
      { agentName: lazy.agentName }
    );
    doc.documentElement.setAttribute("contentanalysisactive", "true");
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
    Services.obs.addObserver(this, "quit-application-requested");
  },

  // nsIObserver
  async observe(aSubj, aTopic, _aData) {
    switch (aTopic) {
      case "quit-application-requested": {
        let quitCancelled = false;
        let pendingRequests =
          this.dlpBusyViewsByTopBrowsingContext.getAllRequests();
        if (pendingRequests.length) {
          let messageBody = this.l10n.formatValueSync(
            "contentanalysis-inprogress-quit-message"
          );
          messageBody = messageBody + "\n\n";
          for (const pendingRequest of pendingRequests) {
            let name = this._getResourceNameFromNameOrOperationType(
              this._getResourceNameOrOperationTypeFromRequest(
                pendingRequest,
                true
              )
            );
            messageBody = messageBody + name + "\n";
          }
          let buttonSelected = Services.prompt.confirmEx(
            null,
            this.l10n.formatValueSync("contentanalysis-inprogress-quit-title"),
            messageBody,
            Ci.nsIPromptService.BUTTON_POS_0 *
              Ci.nsIPromptService.BUTTON_TITLE_IS_STRING +
              Ci.nsIPromptService.BUTTON_POS_1 *
                Ci.nsIPromptService.BUTTON_TITLE_CANCEL +
              Ci.nsIPromptService.BUTTON_POS_0_DEFAULT,
            this.l10n.formatValueSync(
              "contentanalysis-inprogress-quit-yesbutton"
            ),
            null,
            null,
            null,
            { value: 0 }
          );
          if (buttonSelected === 1) {
            aSubj.data = true;
            quitCancelled = true;
          }
        }
        if (!quitCancelled) {
          // Ideally we would wait until "quit-application" to cancel outstanding
          // DLP requests, but the "DLP busy" or "DLP blocked" dialog can block the
          // main thread, thus preventing the "quit-application" from being sent,
          // which causes a shutdownhang. (bug 1899703)
          lazy.gContentAnalysis.cancelAllRequests();
        }
        break;
      }
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
          const analysisType = request.analysisType;
          // For operations that block browser interaction, show the "slow content analysis"
          // dialog faster
          let slowTimeoutMs = this._shouldShowBlockingNotification(analysisType)
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
          let resourceNameOrOperationType =
            this._getResourceNameOrOperationTypeFromRequest(request, false);
          this.requestTokenToRequestInfo.set(request.requestToken, {
            browsingContext,
            resourceNameOrOperationType,
          });
          this.dlpBusyViewsByTopBrowsingContext.setEntry(browsingContext, {
            timer: lazy.setTimeout(() => {
              this.dlpBusyViewsByTopBrowsingContext.setEntry(browsingContext, {
                notification: this._showSlowCAMessage(
                  analysisType,
                  request,
                  resourceNameOrOperationType,
                  browsingContext
                ),
                request,
              });
            }, slowTimeoutMs),
            request,
          });
        }
        break;
      case "dlp-response": {
        const request = aSubj.QueryInterface(Ci.nsIContentAnalysisResponse);
        // Cancels timer or slow message UI,
        // if present, and possibly presents the CA verdict.
        if (!request) {
          throw new Error("Got dlp-response message but no request was passed");
        }

        let windowAndResourceNameOrOperationType =
          this.requestTokenToRequestInfo.get(request.requestToken);
        if (!windowAndResourceNameOrOperationType) {
          // Perhaps this was cancelled just before the response came in from the
          // DLP agent.
          console.warn(
            `Got dlp-response message with unknown token ${request.requestToken}`
          );
          return;
        }
        this.requestTokenToRequestInfo.delete(request.requestToken);
        let dlpBusyView = this.dlpBusyViewsByTopBrowsingContext.getEntry(
          windowAndResourceNameOrOperationType.browsingContext
        );
        if (dlpBusyView) {
          this._disconnectFromView(dlpBusyView);
          this.dlpBusyViewsByTopBrowsingContext.deleteEntry(
            windowAndResourceNameOrOperationType.browsingContext
          );
        }
        const responseResult =
          request?.action ?? Ci.nsIContentAnalysisResponse.eUnspecified;
        await this._showCAResult(
          windowAndResourceNameOrOperationType.resourceNameOrOperationType,
          windowAndResourceNameOrOperationType.browsingContext,
          request.requestToken,
          responseResult,
          request.cancelError
        );
        this._showAnotherPendingDialog(
          windowAndResourceNameOrOperationType.browsingContext
        );
        break;
      }
    }
  },

  async showPanel(element, panelUI) {
    element.ownerDocument.l10n.setAttributes(
      lazy.PanelMultiView.getViewNode(
        element.ownerDocument,
        "content-analysis-panel-description"
      ),
      "content-analysis-panel-text",
      { agentName: lazy.agentName }
    );
    panelUI.showSubView("content-analysis-panel", element);
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
          args.resourceNameOrOperationType
        ),
        request: args.request,
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
      let topWindow =
        aBrowsingContext.topChromeWindow ??
        aBrowsingContext.embedderWindowGlobal.browsingContext.topChromeWindow;
      const notification = new topWindow.Notification(
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

  _shouldShowBlockingNotification(aAnalysisType) {
    return !(
      aAnalysisType == Ci.nsIContentAnalysisRequest.eFileDownloaded ||
      aAnalysisType == Ci.nsIContentAnalysisRequest.ePrint
    );
  },

  // This function also transforms the nameOrOperationType so we won't have to
  // look it up again.
  _getResourceNameFromNameOrOperationType(nameOrOperationType) {
    if (!nameOrOperationType.name) {
      let l10nId = undefined;
      switch (nameOrOperationType.operationType) {
        case Ci.nsIContentAnalysisRequest.eClipboard:
          l10nId = "contentanalysis-operationtype-clipboard";
          break;
        case Ci.nsIContentAnalysisRequest.eDroppedText:
          l10nId = "contentanalysis-operationtype-dropped-text";
          break;
        case Ci.nsIContentAnalysisRequest.eOperationPrint:
          l10nId = "contentanalysis-operationtype-print";
          break;
      }
      if (!l10nId) {
        console.error(
          "Unknown operationTypeForDisplay: " +
            nameOrOperationType.operationType
        );
        return "";
      }
      nameOrOperationType.name = this.l10n.formatValueSync(l10nId);
    }
    return nameOrOperationType.name;
  },

  /**
   * Gets a name or operation type from a request
   *
   * @param {object} aRequest The nsIContentAnalysisRequest
   * @param {boolean} aStandalone Whether the message is going to be used on its own
   *                              line. This is used to add more context to the message
   *                              if a file is being uploaded rather than just the name
   *                              of the file.
   * @returns {object} An object with either a name property that can be used as-is, or
   *                   an operationType property.
   */
  _getResourceNameOrOperationTypeFromRequest(aRequest, aStandalone) {
    if (
      aRequest.operationTypeForDisplay ==
      Ci.nsIContentAnalysisRequest.eCustomDisplayString
    ) {
      if (aStandalone) {
        return {
          name: this.l10n.formatValueSync(
            "contentanalysis-customdisplaystring-description",
            {
              filename: aRequest.operationDisplayString,
            }
          ),
        };
      }
      return { name: aRequest.operationDisplayString };
    }
    return { operationType: aRequest.operationTypeForDisplay };
  },

  /**
   * Show a message to the user to indicate that a CA request is taking
   * a long time.
   */
  _showSlowCAMessage(
    aOperation,
    aRequest,
    aResourceNameOrOperationType,
    aBrowsingContext
  ) {
    if (!this._shouldShowBlockingNotification(aOperation)) {
      return this._showMessage(
        this._getSlowDialogMessage(aResourceNameOrOperationType),
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
          resourceNameOrOperationType: aResourceNameOrOperationType,
        },
      };
    }

    return this._showSlowCABlockingMessage(
      aBrowsingContext,
      aRequest.requestToken,
      aResourceNameOrOperationType
    );
  },

  _getSlowDialogMessage(aResourceNameOrOperationType) {
    if (aResourceNameOrOperationType.name) {
      return this.l10n.formatValueSync(
        "contentanalysis-slow-agent-dialog-body-file",
        {
          agent: lazy.agentName,
          filename: aResourceNameOrOperationType.name,
        }
      );
    }
    let l10nId = undefined;
    switch (aResourceNameOrOperationType.operationType) {
      case Ci.nsIContentAnalysisRequest.eClipboard:
        l10nId = "contentanalysis-slow-agent-dialog-body-clipboard";
        break;
      case Ci.nsIContentAnalysisRequest.eDroppedText:
        l10nId = "contentanalysis-slow-agent-dialog-body-dropped-text";
        break;
      case Ci.nsIContentAnalysisRequest.eOperationPrint:
        l10nId = "contentanalysis-slow-agent-dialog-body-print";
        break;
    }
    if (!l10nId) {
      console.error(
        "Unknown operationTypeForDisplay: ",
        aResourceNameOrOperationType
      );
      return "";
    }
    return this.l10n.formatValueSync(l10nId, {
      agent: lazy.agentName,
    });
  },

  _getErrorDialogMessage(aResourceNameOrOperationType) {
    if (aResourceNameOrOperationType.name) {
      return this.l10n.formatValueSync(
        "contentanalysis-error-message-upload-file",
        {
          filename: aResourceNameOrOperationType.name,
        }
      );
    }
    let l10nId = undefined;
    switch (aResourceNameOrOperationType.operationType) {
      case Ci.nsIContentAnalysisRequest.eClipboard:
        l10nId = "contentanalysis-error-message-clipboard";
        break;
      case Ci.nsIContentAnalysisRequest.eDroppedText:
        l10nId = "contentanalysis-error-message-dropped-text";
        break;
      case Ci.nsIContentAnalysisRequest.eOperationPrint:
        l10nId = "contentanalysis-error-message-print";
        break;
    }
    if (!l10nId) {
      console.error(
        "Unknown operationTypeForDisplay: ",
        aResourceNameOrOperationType
      );
      return "";
    }
    return this.l10n.formatValueSync(l10nId);
  },
  _showSlowCABlockingMessage(
    aBrowsingContext,
    aRequestToken,
    aResourceNameOrOperationType
  ) {
    let bodyMessage = this._getSlowDialogMessage(aResourceNameOrOperationType);
    let promise = Services.prompt.asyncConfirmEx(
      aBrowsingContext,
      Ci.nsIPromptService.MODAL_TYPE_TAB,
      this.l10n.formatValueSync("contentanalysis-slow-agent-dialog-header"),
      bodyMessage,
      Ci.nsIPromptService.BUTTON_POS_0 *
        Ci.nsIPromptService.BUTTON_TITLE_CANCEL +
        Ci.nsIPromptService.BUTTON_POS_1_DEFAULT +
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
    aResourceNameOrOperationType,
    aBrowsingContext,
    aRequestToken,
    aCAResult,
    aRequestCancelError
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
            content: this._getResourceNameFromNameOrOperationType(
              aResourceNameOrOperationType
            ),
            response: "REPORT_ONLY",
          }
        );
        timeoutMs = this._RESULT_NOTIFICATION_FAST_TIMEOUT_MS;
        break;
      case Ci.nsIContentAnalysisResponse.eWarn: {
        const result = await Services.prompt.asyncConfirmEx(
          aBrowsingContext,
          Ci.nsIPromptService.MODAL_TYPE_TAB,
          await this.l10n.formatValue("contentanalysis-warndialogtitle"),
          await this.l10n.formatValue("contentanalysis-warndialogtext", {
            content: this._getResourceNameFromNameOrOperationType(
              aResourceNameOrOperationType
            ),
          }),
          Ci.nsIPromptService.BUTTON_POS_0 *
            Ci.nsIPromptService.BUTTON_TITLE_IS_STRING +
            Ci.nsIPromptService.BUTTON_POS_1 *
              Ci.nsIPromptService.BUTTON_TITLE_IS_STRING +
            Ci.nsIPromptService.BUTTON_POS_2_DEFAULT,
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
      }
      case Ci.nsIContentAnalysisResponse.eBlock: {
        if (!lazy.showBlockedResult) {
          // Don't show anything
          return null;
        }
        let titleId = undefined;
        let body = undefined;
        if (aResourceNameOrOperationType.name) {
          titleId = "contentanalysis-block-dialog-title-upload-file";
          body = this.l10n.formatValueSync(
            "contentanalysis-block-dialog-body-upload-file",
            {
              filename: aResourceNameOrOperationType.name,
            }
          );
        } else {
          let bodyId = undefined;
          switch (aResourceNameOrOperationType.operationType) {
            case Ci.nsIContentAnalysisRequest.eClipboard:
              titleId = "contentanalysis-block-dialog-title-clipboard";
              bodyId = "contentanalysis-block-dialog-body-clipboard";
              break;
            case Ci.nsIContentAnalysisRequest.eDroppedText:
              titleId = "contentanalysis-block-dialog-title-dropped-text";
              bodyId = "contentanalysis-block-dialog-body-dropped-text";
              break;
            case Ci.nsIContentAnalysisRequest.eOperationPrint:
              titleId = "contentanalysis-block-dialog-title-print";
              bodyId = "contentanalysis-block-dialog-body-print";
              break;
          }
          if (!titleId || !bodyId) {
            console.error(
              "Unknown operationTypeForDisplay: ",
              aResourceNameOrOperationType
            );
            return null;
          }
          body = this.l10n.formatValueSync(bodyId);
        }
        let alertBrowsingContext = aBrowsingContext;
        if (aBrowsingContext.embedderElement?.getAttribute("printpreview")) {
          // If we're in a print preview dialog, things are tricky.
          // The window itself is about to close (because of the thrown NS_ERROR_CONTENT_BLOCKED),
          // so using an async call would just immediately make the dialog disappear. (bug 1899714)
          // Using a blocking version can cause a hang if the window is resizing while
          // we show the dialog. (bug 1900798)
          // So instead, try to find the browser that this print preview dialog is on top of
          // and show the dialog there.
          let printPreviewBrowser = aBrowsingContext.embedderElement;
          let win = printPreviewBrowser.ownerGlobal;
          for (let browser of win.gBrowser.browsers) {
            if (
              win.PrintUtils.getPreviewBrowser(browser)?.browserId ===
              printPreviewBrowser.browserId
            ) {
              alertBrowsingContext = browser.browsingContext;
              break;
            }
          }
        }
        await Services.prompt.asyncAlert(
          alertBrowsingContext,
          Ci.nsIPromptService.MODAL_TYPE_TAB,
          this.l10n.formatValueSync(titleId),
          body
        );
        return null;
      }
      case Ci.nsIContentAnalysisResponse.eUnspecified:
        message = await this.l10n.formatValue(
          "contentanalysis-unspecified-error-message-content",
          {
            agent: lazy.agentName,
            content: this._getErrorDialogMessage(aResourceNameOrOperationType),
          }
        );
        timeoutMs = this._RESULT_NOTIFICATION_TIMEOUT_MS;
        break;
      case Ci.nsIContentAnalysisResponse.eCanceled:
        {
          let messageId;
          switch (aRequestCancelError) {
            case Ci.nsIContentAnalysisResponse.eUserInitiated:
              console.error(
                "Got unexpected cancel response with eUserInitiated"
              );
              return null;
            case Ci.nsIContentAnalysisResponse.eNoAgent:
              messageId = "contentanalysis-no-agent-connected-message-content";
              break;
            case Ci.nsIContentAnalysisResponse.eInvalidAgentSignature:
              messageId =
                "contentanalysis-invalid-agent-signature-message-content";
              break;
            case Ci.nsIContentAnalysisResponse.eErrorOther:
              messageId = "contentanalysis-unspecified-error-message-content";
              break;
            default:
              console.error(
                "Unexpected CA cancelError value: " + aRequestCancelError
              );
              messageId = "contentanalysis-unspecified-error-message-content";
              break;
          }
          message = await this.l10n.formatValue(messageId, {
            agent: lazy.agentName,
            content: this._getErrorDialogMessage(aResourceNameOrOperationType),
          });
          timeoutMs = this._RESULT_NOTIFICATION_TIMEOUT_MS;
        }
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
