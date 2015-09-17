/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.models = (function(l10n) {
  "use strict";

  /**
   * Notification model.
   */
  var NotificationModel = Backbone.Model.extend({
    defaults: {
      details: "",
      detailsButtonLabel: "",
      detailsButtonCallback: null,
      level: "info",
      message: ""
    }
  });

  /**
   * Notification collection
   */
  var NotificationCollection = Backbone.Collection.extend({
    model: NotificationModel,

    /**
     * Adds a warning notification to the stack and renders it.
     *
     * @return {String} message
     */
    warn: function(message) {
      this.add({level: "warning", message: message});
    },

    /**
     * Adds a l10n warning notification to the stack and renders it.
     *
     * @param  {String} messageId L10n message id
     */
    warnL10n: function(messageId) {
      this.warn(l10n.get(messageId));
    },

    /**
     * Adds an error notification to the stack and renders it.
     *
     * @return {String} message
     */
    error: function(message) {
      this.add({level: "error", message: message});
    },

    /**
     * Adds a l10n error notification to the stack and renders it.
     *
     * @param  {String} messageId L10n message id
     * @param  {Object} [l10nProps] An object with variables to be interpolated
     *                  into the translation. All members' values must be
     *                  strings or numbers.
     */
    errorL10n: function(messageId, l10nProps) {
      this.error(l10n.get(messageId, l10nProps));
    },

    /**
     * Adds a success notification to the stack and renders it.
     *
     * @return {String} message
     */
    success: function(message) {
      this.add({level: "success", message: message});
    },

    /**
     * Adds a l10n success notification to the stack and renders it.
     *
     * @param  {String} messageId L10n message id
     * @param  {Object} [l10nProps] An object with variables to be interpolated
     *                  into the translation. All members' values must be
     *                  strings or numbers.
     */
    successL10n: function(messageId, l10nProps) {
      this.success(l10n.get(messageId, l10nProps));
    }
  });

  return {
    NotificationCollection: NotificationCollection,
    NotificationModel: NotificationModel
  };
})(navigator.mozL10n || document.mozL10n);
