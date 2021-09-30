/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  setMocksInGlobal,
} = require("devtools/client/shared/test-helpers/shared-node-helpers");
setMocksInGlobal();

const { LocalizationHelper } = require("devtools/shared/l10n");
global.L10N = new LocalizationHelper(
  "devtools/client/locales/debugger.properties"
);

const { URL } = require("url");
global.URL = URL;

// JSDOM doesn't seem to have those functions that are used by codeMirror.
// See https://github.com/jsdom/jsdom/issues/3002
document.createRange = () => {
  const range = new Range();

  range.getBoundingClientRect = jest.fn();

  range.getClientRects = jest.fn(() => ({
    item: () => null,
    length: 0,
  }));

  return range;
};
