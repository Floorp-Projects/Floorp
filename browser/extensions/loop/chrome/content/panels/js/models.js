"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.panel = loop.panel || {};
loop.panel.models = function () {
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
      message: "" } });



  /**
   * Notification collection
   */
  var NotificationCollection = Backbone.Collection.extend({ 
    model: NotificationModel });


  return { 
    NotificationCollection: NotificationCollection, 
    NotificationModel: NotificationModel };}();
