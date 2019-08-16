/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  ObjectProvider,
} = require("devtools/client/shared/components/tree/ObjectProvider");
const {
  getFileName,
} = require("devtools/client/netmonitor/src/utils/request-utils");

/**
 * This provider is responsible for providing data from the
 * search reducer to the SearchPanel.
 */
const SearchProvider = {
  ...ObjectProvider,

  getChildren(object) {
    if (Array.isArray(object)) {
      return object;
    } else if (object.resource) {
      return object.results;
    } else if (object.type) {
      return [];
    }
    return ObjectProvider.getLabel(object);
  },

  hasChildren(object) {
    return this.getChildren(object).length > 0;
  },

  getLabel(object) {
    if (object.resource) {
      return this.getResourceLabel(object);
    } else if (object.type) {
      switch (object.type) {
        case "url":
          return this.getUrlLabel(object);
        case "responseContent":
          return this.getResponseContent(object);
        case "requestCookies":
          return this.getRequestCookies();
        case "responseCookies":
          return this.getResponseCookies();
        case "requestHeaders":
          return this.getRequestHeaders();
        case "responseHeaders":
          return this.getResponseHeaders();
      }
    }
    return ObjectProvider.getLabel(object);
  },

  getValue(object) {
    if (object.resource) {
      return "";
    } else if (object.type) {
      return object.value;
    }
    return ObjectProvider.getValue(object);
  },

  getKey(object) {
    if (object.resource) {
      return object.resource.id;
    } else if (object.type) {
      return object.key;
    }
    return ObjectProvider.getKey(object);
  },

  getType(object) {
    if (object.resource) {
      return "resource";
    } else if (object.type) {
      return "result";
    }
    return ObjectProvider.getType(object);
  },

  getResourceLabel(object) {
    return (
      getFileName(object.resource.urlDetails.baseNameWithQuery) ||
      object.resource.urlDetails.host
    );
  },

  getUrlLabel(object) {
    return object.label;
  },

  getResponseContent(object) {
    return object.line + "";
  },

  getRequestCookies() {
    return "Set-Cookie";
  },

  getResponseCookies() {
    return "Cookie";
  },

  getRequestHeaders() {
    return L10N.getStr("netmonitor.search.labels.requestHeaders");
  },

  getResponseHeaders() {
    return L10N.getStr("netmonitor.search.labels.responseHeaders");
  },
};

module.exports = {
  SearchProvider,
};
