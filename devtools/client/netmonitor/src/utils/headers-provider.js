/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ObjectProvider } = require("devtools/client/shared/components/tree/ObjectProvider");

/**
 * Custom tree provider.
 *
 * This provider is used to provide set of headers and is
 * utilized by the HeadersPanel.
 * The default ObjectProvider can't be used since it doesn't
 * allow duplicities by design and so it can't support duplicity
 * headers (more headers with the same name).
 */
var HeadersProvider = {
  ...ObjectProvider,

  getChildren(object) {
    if (object.value instanceof HeaderList) {
      return object.value.headers.map((header, index) =>
        new Header(header.name, header.value, index));
    }
    return ObjectProvider.getChildren(object);
  },

  hasChildren: function (object) {
    if (object.value instanceof HeaderList) {
      return object.value.headers.length > 0;
    } else if (object instanceof Header) {
      return false;
    }
    return ObjectProvider.hasChildren(object);
  },

  getLabel: function (object) {
    if (object instanceof Header) {
      return object.name;
    }
    return ObjectProvider.getLabel(object);
  },

  getValue: function (object) {
    if (object instanceof Header) {
      return object.value;
    }
    return ObjectProvider.getValue(object);
  },

  getKey(object) {
    if (object instanceof Header) {
      return object.key;
    }
    return ObjectProvider.getKey(object);
  },

  getType: function (object) {
    if (object instanceof Header) {
      return "string";
    }
    return ObjectProvider.getType(object);
  }
};

/**
 * Helper data structures for list of headers.
 */
function HeaderList(headers) {
  this.headers = headers;
  this.headers.sort((a, b) => {
    return a.name.toLowerCase().localeCompare(b.name.toLowerCase());
  });
}

function Header(name, value, key) {
  this.name = name;
  this.value = value;
  this.key = key;
}

module.exports = {
  HeadersProvider,
  HeaderList
};
