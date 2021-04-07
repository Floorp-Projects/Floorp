/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PromptParent"];

ChromeUtils.defineModuleGetter(
  this,
  "PromptUtils",
  "resource://gre/modules/SharedPromptUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "tabChromePromptSubDialog",
  "prompts.tabChromePromptSubDialog",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "contentPromptSubDialog",
  "prompts.contentPromptSubDialog",
  false
);

XPCOMUtils.defineLazyGetter(this, "gTabBrowserLocalization", () => {
  return new Localization(["browser/tabbrowser.ftl"], true);
});

/**
 * @typedef {Object} Prompt
 * @property {Function} resolver
 *           The resolve function to be called with the data from the Prompt
 *           after the user closes it.
 * @property {Object} tabModalPrompt
 *           The TabModalPrompt being shown to the user.
 */

/**
 * gBrowserPrompts weakly maps BrowsingContexts to a Map of their currently
 * active Prompts.
 *
 * @type {WeakMap<BrowsingContext, Prompt>}
 */
let gBrowserPrompts = new WeakMap();

class PromptParent extends JSWindowActorParent {
  didDestroy() {
    // In the event that the subframe or tab crashed, make sure that
    // we close any active Prompts.
    this.forceClosePrompts();
  }

  /**
   * Registers a new Prompt to be tracked for a particular BrowsingContext.
   * We need to track a Prompt so that we can, for example, force-close the
   * TabModalPrompt if the originating subframe or tab unloads or crashes.
   *
   * @param {Object} tabModalPrompt
   *        The TabModalPrompt that will be shown to the user.
   * @param {string} id
   *        A unique ID to differentiate multiple Prompts coming from the same
   *        BrowsingContext.
   * @return {Promise}
   * @resolves {Object}
   *           Resolves with the arguments returned from the TabModalPrompt when it
   *           is dismissed.
   */
  registerPrompt(tabModalPrompt, id) {
    let prompts = gBrowserPrompts.get(this.browsingContext);
    if (!prompts) {
      prompts = new Map();
      gBrowserPrompts.set(this.browsingContext, prompts);
    }

    let promise = new Promise(resolve => {
      prompts.set(id, {
        tabModalPrompt,
        resolver: resolve,
      });
    });

    return promise;
  }

  /**
   * Removes a Prompt for a BrowsingContext with a particular ID from the registry.
   * This needs to be done to avoid leaking <xul:browser>'s.
   *
   * @param {string} id
   *        A unique ID to differentiate multiple Prompts coming from the same
   *        BrowsingContext.
   */
  unregisterPrompt(id) {
    let prompts = gBrowserPrompts.get(this.browsingContext);
    if (prompts) {
      prompts.delete(id);
    }
  }

  /**
   * Programmatically closes all Prompts for the current BrowsingContext.
   */
  forceClosePrompts() {
    let prompts = gBrowserPrompts.get(this.browsingContext) || [];

    for (let [, prompt] of prompts) {
      prompt.tabModalPrompt && prompt.tabModalPrompt.abortPrompt();
    }
  }

  isAboutAddonsOptionsPage(browsingContext) {
    const { embedderWindowGlobal, name } = browsingContext;
    if (!embedderWindowGlobal) {
      // Return earlier if there is no embedder global, this is definitely
      // not an about:addons extensions options page.
      return false;
    }

    return (
      embedderWindowGlobal.documentPrincipal.isSystemPrincipal &&
      embedderWindowGlobal.documentURI.spec === "about:addons" &&
      name === "addon-inline-options"
    );
  }

  receiveMessage(message) {
    let args = message.data;
    let id = args._remoteId;

    switch (message.name) {
      case "Prompt:Open": {
        if (
          (args.modalType === Ci.nsIPrompt.MODAL_TYPE_CONTENT &&
            !contentPromptSubDialog) ||
          (args.modalType === Ci.nsIPrompt.MODAL_TYPE_TAB &&
            !tabChromePromptSubDialog) ||
          this.isAboutAddonsOptionsPage(this.browsingContext)
        ) {
          return this.openContentPrompt(args, id);
        }
        return this.openPromptWithTabDialogBox(args);
      }
    }

    return undefined;
  }

  /**
   * Opens a TabModalPrompt for a BrowsingContext, and puts the associated browser
   * in the modal state until the TabModalPrompt is closed.
   *
   * @param {Object} args
   *        The arguments passed up from the BrowsingContext to be passed directly
   *        to the TabModalPrompt.
   * @param {string} id
   *        A unique ID to differentiate multiple Prompts coming from the same
   *        BrowsingContext.
   * @return {Promise}
   *         Resolves when the TabModalPrompt is dismissed.
   * @resolves {Object}
   *           The arguments returned from the TabModalPrompt.
   */
  openContentPrompt(args, id) {
    let browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      throw new Error("Cannot tab-prompt without a browser!");
    }
    let window = browser.ownerGlobal;
    let tabPrompt = window.gBrowser.getTabModalPromptBox(browser);
    let newPrompt;
    let needRemove = false;

    // If the page which called the prompt is different from the the top context
    // where we show the prompt, ask the prompt implementation to display the origin.
    // For example, this can happen if a cross origin subframe shows a prompt.
    args.showCallerOrigin =
      args.promptPrincipal &&
      !browser.contentPrincipal.equals(args.promptPrincipal);

    let onPromptClose = () => {
      let promptData = gBrowserPrompts.get(this.browsingContext);
      if (!promptData || !promptData.has(id)) {
        throw new Error(
          "Failed to close a prompt since it wasn't registered for some reason."
        );
      }

      let { resolver, tabModalPrompt } = promptData.get(id);
      // It's possible that we removed the prompt during the
      // appendPrompt call below. In that case, newPrompt will be
      // undefined. We set the needRemove flag to remember to remove
      // it right after we've finished adding it.
      if (tabModalPrompt) {
        tabPrompt.removePrompt(tabModalPrompt);
      } else {
        needRemove = true;
      }

      this.unregisterPrompt(id);

      PromptUtils.fireDialogEvent(
        window,
        "DOMModalDialogClosed",
        browser,
        this.getClosingEventDetail(args)
      );
      resolver(args);
      browser.maybeLeaveModalState();
    };

    try {
      browser.enterModalState();
      PromptUtils.fireDialogEvent(
        window,
        "DOMWillOpenModalDialog",
        browser,
        this.getOpenEventDetail(args)
      );

      args.promptActive = true;

      newPrompt = tabPrompt.appendPrompt(args, onPromptClose);
      let promise = this.registerPrompt(newPrompt, id);

      if (needRemove) {
        tabPrompt.removePrompt(newPrompt);
      }

      return promise;
    } catch (ex) {
      Cu.reportError(ex);
      onPromptClose(true);
    }

    return null;
  }

  /**
   * Opens either a window prompt or TabDialogBox at the content or tab level
   * for a BrowsingContext, and puts the associated browser in the modal state
   * until the prompt is closed.
   *
   * @param {Object} args
   *        The arguments passed up from the BrowsingContext to be passed
   *        directly to the modal prompt.
   * @return {Promise}
   *         Resolves when the modal prompt is dismissed.
   * @resolves {Object}
   *           The arguments returned from the modal prompt.
   */
  async openPromptWithTabDialogBox(args) {
    const COMMON_DIALOG = "chrome://global/content/commonDialog.xhtml";
    const SELECT_DIALOG = "chrome://global/content/selectDialog.xhtml";
    let uri = args.promptType == "select" ? SELECT_DIALOG : COMMON_DIALOG;

    let browsingContext = this.browsingContext.top;

    let browser = browsingContext.embedderElement;
    let win;

    // If we are a chrome actor we can use the associated chrome win.
    if (!browsingContext.isContent && browsingContext.window) {
      win = browsingContext.window;
    } else {
      win = browser?.ownerGlobal;
    }

    // There's a requirement for prompts to be blocked if a window is
    // passed and that window is hidden (eg, auth prompts are suppressed if the
    // passed window is the hidden window).
    // See bug 875157 comment 30 for more..
    if (win?.winUtils && !win.winUtils.isParentWindowMainWidgetVisible) {
      throw new Error("Cannot call openModalWindow on a hidden window");
    }

    try {
      if (browser) {
        browser.enterModalState();
        PromptUtils.fireDialogEvent(
          win,
          "DOMWillOpenModalDialog",
          browser,
          this.getOpenEventDetail(args)
        );
      }

      args.promptAborted = false;
      args.openedWithTabDialog = true;

      // Convert args object to a prop bag for the dialog to consume.
      let bag;

      if (
        (args.modalType === Services.prompt.MODAL_TYPE_TAB ||
          args.modalType === Services.prompt.MODAL_TYPE_CONTENT) &&
        win?.gBrowser?.getTabDialogBox
      ) {
        if (!browser) {
          let modal_type =
            args.modalType === Services.prompt.MODAL_TYPE_TAB
              ? "tab"
              : "content";
          throw new Error(`Cannot ${modal_type}-prompt without a browser!`);
        }
        // Tab or content level prompt
        let dialogBox = win.gBrowser.getTabDialogBox(browser);

        if (dialogBox._allowTabFocusByPromptPrincipal) {
          this.addTabSwitchCheckboxToArgs(dialogBox, args);
        }

        bag = PromptUtils.objectToPropBag(args);
        await dialogBox.open(
          uri,
          {
            features: "resizable=no",
            modalType: args.modalType,
            allowFocusCheckbox: args.allowFocusCheckbox,
          },
          bag
        );
      } else {
        // Ensure we set the correct modal type at this point.
        // If we use window prompts as a fallback it may not be set.
        args.modalType = Services.prompt.MODAL_TYPE_WINDOW;
        // Window prompt
        bag = PromptUtils.objectToPropBag(args);
        Services.ww.openWindow(
          win,
          uri,
          "_blank",
          "centerscreen,chrome,modal,titlebar",
          bag
        );
      }

      PromptUtils.propBagToObject(bag, args);
    } finally {
      if (browser) {
        browser.maybeLeaveModalState();
        PromptUtils.fireDialogEvent(
          win,
          "DOMModalDialogClosed",
          browser,
          this.getClosingEventDetail(args)
        );
      }
    }
    return args;
  }

  getClosingEventDetail(args) {
    let details =
      args.modalType === Services.prompt.MODAL_TYPE_CONTENT
        ? {
            wasPermitUnload: args.inPermitUnload,
            areLeaving: args.ok,
          }
        : null;

    return details;
  }

  getOpenEventDetail(args) {
    let details =
      args.modalType === Services.prompt.MODAL_TYPE_CONTENT
        ? {
            inPermitUnload: args.inPermitUnload,
            promptPrincipal: args.promptPrincipal,
            tabPrompt: true,
          }
        : null;

    return details;
  }

  /**
   * Set properties on `args` needed by the dialog to allow tab switching for the
   * page that opened the prompt.
   *
   * @param {TabDialogBox}  dialogBox
   *        The dialog to show the tab-switch checkbox for.
   * @param {Object}  args
   *        The `args` object to set tab switching permission info on.
   */
  addTabSwitchCheckboxToArgs(dialogBox, args) {
    let allowTabFocusByPromptPrincipal =
      dialogBox._allowTabFocusByPromptPrincipal;

    if (
      allowTabFocusByPromptPrincipal &&
      args.modalType === Services.prompt.MODAL_TYPE_CONTENT
    ) {
      let [allowFocusMsg] = gTabBrowserLocalization.formatMessagesSync([
        {
          id: "tabbrowser-allow-dialogs-to-get-focus",
          args: {
            domain: allowTabFocusByPromptPrincipal.URI.host,
          },
        },
      ]);
      let labelAttr = allowFocusMsg.attributes.find(a => a.name == "label");
      if (labelAttr) {
        args.allowFocusCheckbox = true;
        args.checkLabel = labelAttr.value;
      }
    }
  }
}
