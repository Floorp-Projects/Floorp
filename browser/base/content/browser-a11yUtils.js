/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

/**
 * Utility functions for UI accessibility.
 */

var A11yUtils = {
  /**
   * Announce a message to the user.
   * This should only be used when something happens that is important to the
   * user and will be noticed visually, but is not related to the focused
   * control and is not a pop-up such as a doorhanger.
   * For example, this could be used to indicate that Reader View is available
   * or that Firefox is making a recommendation via the toolbar.
   * This must be used with caution, as it can create unwanted verbosity and
   * can thus hinder rather than help users if used incorrectly.
   * Please only use this after consultation with the Mozilla accessibility
   * team.
   * @param {object} [options]
   * @param {string} [options.id] The Fluent id of the message to announce. The
   *        ftl file must already be included in browser.xhtml. This must be
   *        specified unless a raw message is specified instead.
   * @param {object} [options.args] Arguments for the Fluent message.
   * @param {string} [options.raw] The raw, already localized message to
   *        announce. You should generally prefer a Fluent id instead, but in
   *        rare cases, this might not be feasible.
   */
  async announce({ id = null, args = {}, raw = null } = {}) {
    if ((!id && !raw) || (id && raw)) {
      throw new Error("One of raw or id must be specified.");
    }

    // Cancel a previous pending call if any.
    if (this._cancelAnnounce) {
      this._cancelAnnounce();
      this._cancelAnnounce = null;
    }

    let message;
    if (id) {
      let cancel = false;
      this._cancelAnnounce = () => (cancel = true);
      message = await document.l10n.formatValue(id, args);
      if (cancel) {
        // announce() was called again while we were waiting for translation.
        return;
      }
      // No more async operations from this point.
      this._cancelAnnounce = null;
    } else {
      // We run fully synchronously if a raw message is provided.
      message = raw;
    }

    // For now, we don't use source, but it might be useful in future.
    // For example, we might use it when we support announcement events on
    // more platforms or it could be used to have a keyboard shortcut which
    // focuses the last element to announce a message.
    let live = document.getElementById("a11y-announcement");
    // We use role="alert" because JAWS doesn't support aria-live in browser
    // chrome.
    // Gecko a11y needs an insertion to trigger an alert event. This is why
    // we can't just use aria-label on the alert.
    if (live.firstChild) {
      live.firstChild.remove();
    }
    let label = document.createElement("label");
    label.setAttribute("aria-label", message);
    live.appendChild(label);
  },
};
