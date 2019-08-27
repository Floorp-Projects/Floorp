/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
    } else if (object.label) {
      return object.label;
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

  getResourceTooltipLabel(object) {
    const { resource } = object;
    if (resource.urlDetails && resource.urlDetails.url) {
      return resource.urlDetails.url;
    }

    return this.getResourceLabel(object);
  },

  getResourceLabel(object) {
    return (
      getFileName(object.resource.urlDetails.baseNameWithQuery) ||
      object.resource.urlDetails.host
    );
  },
};

module.exports = {
  SearchProvider,
};
