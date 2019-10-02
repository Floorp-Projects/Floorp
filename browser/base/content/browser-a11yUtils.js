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
   * @param aMessage The message to announce.
   * @param aSource The element with which the announcement is associated.
   *        This should generally be something the user can interact with to
   *        respond to the announcement.
   *        For example, for an announcement indicating that Reader View is
   *        available, this should be the Reader View button on the toolbar.
   */
  announce(aMessage, aSource = document) {
    // For now, we don't use aSource, but it might be useful in future.
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
    label.setAttribute("aria-label", aMessage);
    live.appendChild(label);
  },
};
