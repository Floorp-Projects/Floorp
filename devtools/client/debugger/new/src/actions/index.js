"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _breakpoints = require("./breakpoints");

var breakpoints = _interopRequireWildcard(_breakpoints);

var _expressions = require("./expressions");

var expressions = _interopRequireWildcard(_expressions);

var _eventListeners = require("./event-listeners");

var eventListeners = _interopRequireWildcard(_eventListeners);

var _pause = require("./pause/index");

var pause = _interopRequireWildcard(_pause);

var _navigation = require("./navigation");

var navigation = _interopRequireWildcard(_navigation);

var _ui = require("./ui");

var ui = _interopRequireWildcard(_ui);

var _fileSearch = require("./file-search");

var fileSearch = _interopRequireWildcard(_fileSearch);

var _ast = require("./ast");

var ast = _interopRequireWildcard(_ast);

var _coverage = require("./coverage");

var coverage = _interopRequireWildcard(_coverage);

var _projectTextSearch = require("./project-text-search");

var projectTextSearch = _interopRequireWildcard(_projectTextSearch);

var _replay = require("./replay");

var replay = _interopRequireWildcard(_replay);

var _quickOpen = require("./quick-open");

var quickOpen = _interopRequireWildcard(_quickOpen);

var _sourceTree = require("./source-tree");

var sourceTree = _interopRequireWildcard(_sourceTree);

var _sources = require("./sources/index");

var sources = _interopRequireWildcard(_sources);

var _debuggee = require("./debuggee");

var debuggee = _interopRequireWildcard(_debuggee);

var _toolbox = require("./toolbox");

var toolbox = _interopRequireWildcard(_toolbox);

var _preview = require("./preview");

var preview = _interopRequireWildcard(_preview);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
exports.default = { ...navigation,
  ...breakpoints,
  ...expressions,
  ...eventListeners,
  ...sources,
  ...pause,
  ...ui,
  ...fileSearch,
  ...ast,
  ...coverage,
  ...projectTextSearch,
  ...replay,
  ...quickOpen,
  ...sourceTree,
  ...debuggee,
  ...toolbox,
  ...preview
};