"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.addTab = addTab;
exports.moveTab = moveTab;
exports.closeTab = closeTab;
exports.closeTabs = closeTabs;

var _editor = require("../../utils/editor/index");

var _sources = require("../sources/index");

var _selectors = require("../../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the sources state
 * @module actions/sources
 */
function addTab(url, tabIndex) {
  return {
    type: "ADD_TAB",
    url,
    tabIndex
  };
}

function moveTab(url, tabIndex) {
  return {
    type: "MOVE_TAB",
    url,
    tabIndex
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function closeTab(url) {
  return ({
    dispatch,
    getState,
    client
  }) => {
    (0, _editor.removeDocument)(url);
    const tabs = (0, _selectors.removeSourceFromTabList)((0, _selectors.getSourceTabs)(getState()), url);
    const sourceId = (0, _selectors.getNewSelectedSourceId)(getState(), tabs);
    dispatch({
      type: "CLOSE_TAB",
      url,
      tabs
    });
    dispatch((0, _sources.selectSource)(sourceId));
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function closeTabs(urls) {
  return ({
    dispatch,
    getState,
    client
  }) => {
    urls.forEach(url => {
      const source = (0, _selectors.getSourceByURL)(getState(), url);

      if (source) {
        (0, _editor.removeDocument)(source.id);
      }
    });
    const tabs = (0, _selectors.removeSourcesFromTabList)((0, _selectors.getSourceTabs)(getState()), urls);
    dispatch({
      type: "CLOSE_TABS",
      urls,
      tabs
    });
    const sourceId = (0, _selectors.getNewSelectedSourceId)(getState(), tabs);
    dispatch((0, _sources.selectSource)(sourceId));
  };
}