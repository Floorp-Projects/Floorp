"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _expressions = require("./expressions");

var _expressions2 = _interopRequireDefault(_expressions);

var _eventListeners = require("./event-listeners");

var _eventListeners2 = _interopRequireDefault(_eventListeners);

var _sources = require("./sources");

var _sources2 = _interopRequireDefault(_sources);

var _breakpoints = require("./breakpoints");

var _breakpoints2 = _interopRequireDefault(_breakpoints);

var _pendingBreakpoints = require("./pending-breakpoints");

var _pendingBreakpoints2 = _interopRequireDefault(_pendingBreakpoints);

var _asyncRequests = require("./async-requests");

var _asyncRequests2 = _interopRequireDefault(_asyncRequests);

var _pause = require("./pause");

var _pause2 = _interopRequireDefault(_pause);

var _ui = require("./ui");

var _ui2 = _interopRequireDefault(_ui);

var _fileSearch = require("./file-search");

var _fileSearch2 = _interopRequireDefault(_fileSearch);

var _ast = require("./ast");

var _ast2 = _interopRequireDefault(_ast);

var _coverage = require("./coverage");

var _coverage2 = _interopRequireDefault(_coverage);

var _projectTextSearch = require("./project-text-search");

var _projectTextSearch2 = _interopRequireDefault(_projectTextSearch);

var _replay = require("./replay");

var _replay2 = _interopRequireDefault(_replay);

var _quickOpen = require("./quick-open");

var _quickOpen2 = _interopRequireDefault(_quickOpen);

var _sourceTree = require("./source-tree");

var _sourceTree2 = _interopRequireDefault(_sourceTree);

var _debuggee = require("./debuggee");

var _debuggee2 = _interopRequireDefault(_debuggee);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Reducer index
 * @module reducers/index
 */
exports.default = {
  expressions: _expressions2.default,
  eventListeners: _eventListeners2.default,
  sources: _sources2.default,
  breakpoints: _breakpoints2.default,
  pendingBreakpoints: _pendingBreakpoints2.default,
  asyncRequests: _asyncRequests2.default,
  pause: _pause2.default,
  ui: _ui2.default,
  fileSearch: _fileSearch2.default,
  ast: _ast2.default,
  coverage: _coverage2.default,
  projectTextSearch: _projectTextSearch2.default,
  replay: _replay2.default,
  quickOpen: _quickOpen2.default,
  sourceTree: _sourceTree2.default,
  debuggee: _debuggee2.default
};