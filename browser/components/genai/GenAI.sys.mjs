/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatEnabled",
  "browser.ml.chat.enabled"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatPromptPrefix",
  "browser.ml.chat.prompt.prefix"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatProvider",
  "browser.ml.chat.provider"
);

export const GenAI = {
  /**
   * Build prompts menu to ask chat for context menu or popup.
   *
   * @param {MozMenu} menu Element to update
   * @param {nsContextMenu} context Additional menu context
   */
  buildAskChatMenu(menu, context) {
    if (!lazy.chatEnabled || lazy.chatProvider == "") {
      context.showItem(menu, false);
      return;
    }

    menu.context = context;
    menu.label = "Ask chatbot";
    menu.menupopup?.remove();
    Services.prefs.getChildList("browser.ml.chat.prompts.").forEach(pref => {
      try {
        let prompt = Services.prefs.getStringPref(pref);
        try {
          prompt = JSON.parse(prompt);
        } catch (ex) {}
        menu.appendItem(prompt.label ?? prompt, prompt.value ?? "");
      } catch (ex) {
        console.error("Failed to add menu item for " + pref, ex);
      }
    });
    context.showItem(menu, menu.itemCount > 0);
  },

  /**
   * Build a prompt with context.
   *
   * @param {MozMenuItem} item Use value falling back to label
   * @param {object} context Placeholder keys with values to replace
   * @returns {string} Prompt with placeholders replaced
   */
  buildChatPrompt(item, context = {}) {
    // Combine prompt prefix with the item then replace placeholders from the
    // original prompt (and not from context)
    return (lazy.chatPromptPrefix + (item.value || item.label)).replace(
      // Handle %placeholder% as key|options
      /\%(\w+)(?:\|([^%]+))?\%/g,
      (placeholder, key, options) =>
        // Currently only supporting numeric options for slice with `undefined`
        // resulting in whole string
        context[key]?.slice(0, options) ?? placeholder
    );
  },

  /**
   * Handle selected prompt by opening tab or sidebar.
   *
   * @param {Event} event from menu command
   */
  handleAskChat({ target }) {
    const win = target.ownerGlobal;
    const { selectedTab } = win.gBrowser;
    const url = new URL(lazy.chatProvider);
    url.searchParams.set(
      "q",
      this.buildChatPrompt(target, {
        currentTabTitle:
          (selectedTab._labelIsContentTitle && selectedTab.label) || "",
        selection: target.closest("menu").context.selectionInfo.fullText ?? "",
      })
    );
    win.openWebLinkIn(url + "", "tab", { relatedToCurrent: true });
  },

  /**
   * Build preferences for chat such as handling providers.
   *
   * @param {Window} window for about:preferences
   */
  buildPreferences({ document, Preferences }) {
    const providerEl = document.getElementById("genai-chat-provider");
    if (!providerEl) {
      return;
    }

    const enabled = Preferences.get("browser.ml.chat.enabled");
    const onEnabledChange = () => (providerEl.disabled = !enabled.value);
    onEnabledChange();
    enabled.on("change", onEnabledChange);

    // TODO bug 1895433 populate providers
    Preferences.add({ id: "browser.ml.chat.provider", type: "string" });
  },
};
