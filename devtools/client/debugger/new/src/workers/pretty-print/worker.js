"use strict";

var _prettyFast = require("pretty-fast/index");

var _prettyFast2 = _interopRequireDefault(_prettyFast);

var _devtoolsUtils = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-utils"];

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  workerHandler
} = _devtoolsUtils.workerUtils;

function prettyPrint({
  url,
  indent,
  sourceText
}) {
  const prettified = (0, _prettyFast2.default)(sourceText, {
    url: url,
    indent: " ".repeat(indent)
  });
  return {
    code: prettified.code,
    mappings: invertMappings(prettified.map._mappings)
  };
}

function invertMappings(mappings) {
  return mappings._array.map(m => {
    const mapping = {
      generated: {
        line: m.originalLine,
        column: m.originalColumn
      }
    };

    if (m.source) {
      mapping.source = m.source;
      mapping.original = {
        line: m.generatedLine,
        column: m.generatedColumn
      };
      mapping.name = m.name;
    }

    return mapping;
  });
}

self.onmessage = workerHandler({
  prettyPrint
});