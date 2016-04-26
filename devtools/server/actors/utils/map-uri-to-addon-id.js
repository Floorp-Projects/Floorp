/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const Services = require("Services");

loader.lazyServiceGetter(this, "AddonPathService",
                         "@mozilla.org/addon-path-service;1",
                         "amIAddonPathService");

const B2G_ID = "{3c2e2abc-06d4-11e1-ac3b-374f68613e61}";
const GRAPHENE_ID = "{d1bfe7d9-c01e-4237-998b-7b5f960a4314}";

/**
 * This is a wrapper around amIAddonPathService.mapURIToAddonID which always returns
 * false on B2G and graphene to avoid loading the add-on manager there and
 * reports any exceptions rather than throwing so that the caller doesn't have
 * to worry about them.
 */
if (!Services.appinfo
    || Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT
    || Services.appinfo.ID === undefined /* XPCShell */
    || Services.appinfo.ID == B2G_ID
    || Services.appinfo.ID == GRAPHENE_ID
    || !AddonPathService) {
  module.exports = function mapURIToAddonId(uri) {
    return false;
  };
} else {
  module.exports = function mapURIToAddonId(uri) {
    try {
      return AddonPathService.mapURIToAddonId(uri);
    }
    catch (e) {
      DevToolsUtils.reportException("mapURIToAddonId", e);
      return false;
    }
  };
}
