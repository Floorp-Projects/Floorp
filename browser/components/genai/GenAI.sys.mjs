/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

export const GenAI = {
  /**
   * Build prompts menu to ask chat for context menu or popup.
   *
   * @param {MozMenu} menu Element to update
   * @param {nsContextMenu} context Additional menu context
   */
  buildAskChatMenu(menu, context) {
    context.showItem(menu, menu.itemCount > 0);
  },

  /**
   * Handle selected prompt by opening tab or sidebar.
   */
  handleAskChat() {},
};
