/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable react/prop-types */

"use strict";

const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;

const { FILTER_SEARCH_DELAY } = require("../constants");

// Components
const SearchBox = createFactory(require("devtools/client/shared/components/search-box"));
const TreeViewClass = require("devtools/client/shared/components/tree/tree-view");
const TreeView = createFactory(TreeViewClass);
const TreeRow = createFactory(require("devtools/client/shared/components/tree/tree-row"));
const SourceEditor = createFactory(require("./source-editor"));

const { div, tr, td } = DOM;
const AUTO_EXPAND_MAX_LEVEL = 7;
const AUTO_EXPAND_MAX_NODES = 50;
const EDITOR_CONFIG_ID = "EDITOR_CONFIG";

/*
 * Properties View component
 * A scrollable tree view component which provides some useful features for
 * representing object properties.
 *
 * Search filter - Set enableFilter to enable / disable SearchBox feature.
 * Tree view - Default enabled.
 * Source editor - Enable by specifying object level 1 property name to EDITOR_CONFIG_ID.
 * Rep - Default enabled.
 */
const PropertiesView = createClass({
  displayName: "PropertiesView",

  propTypes: {
    object: PropTypes.object,
    enableInput: PropTypes.bool,
    expandableStrings: PropTypes.bool,
    filterPlaceHolder: PropTypes.string,
    sectionNames: PropTypes.array,
  },

  getDefaultProps() {
    return {
      enableInput: true,
      enableFilter: true,
      expandableStrings: false,
      filterPlaceHolder: "",
      sectionNames: [],
    };
  },

  getInitialState() {
    return {
      filterText: "",
    };
  },

  getRowClass(object, sectionNames) {
    return sectionNames.includes(object.name) ? "tree-section" : "";
  },

  onFilter(object, whiteList) {
    let { name, value } = object;
    let filterText = this.state.filterText;

    if (!filterText || whiteList.includes(name)) {
      return true;
    }

    let jsonString = JSON.stringify({ [name]: value }).toLowerCase();
    return jsonString.includes(filterText.toLowerCase());
  },

  renderRowWithEditor(props) {
    const { level, name, value, path } = props.member;

    // Display source editor when specifying to EDITOR_CONFIG_ID along with config
    if (level === 1 && name === EDITOR_CONFIG_ID) {
      return (
        tr({ key: EDITOR_CONFIG_ID, className: "editor-row-container" },
          td({ colSpan: 2 },
            SourceEditor(value)
          )
        )
      );
    }

    // Skip for editor config
    if (level >= 1 && path.includes(EDITOR_CONFIG_ID)) {
      return null;
    }

    return TreeRow(props);
  },

  renderValueWithRep(props) {
    const { member } = props;

    // Hide strings with following conditions
    // 1. this row is a togglable section and content is object ('cause it shouldn't hide
    //    when string or number)
    // 2. the `value` object has a `value` property, only happened in Cookies panel
    // Put 2 here to not dup this method
    if (member.level === 0 && member.type === "object" ||
      (typeof member.value === "object" && member.value && member.value.value)) {
      return null;
    }

    return Rep(Object.assign(props, {
      // FIXME: A workaround for the issue in StringRep
      // Force StringRep to crop the text everytime
      member: Object.assign({}, member, { open: false }),
      mode: MODE.TINY,
      cropLimit: 60,
    }));
  },

  shouldRenderSearchBox(object) {
    return this.props.enableFilter && object && Object.keys(object)
      .filter((section) => !object[section][EDITOR_CONFIG_ID]).length > 0;
  },

  updateFilterText(filterText) {
    this.setState({
      filterText,
    });
  },

  render() {
    const {
      decorator,
      enableInput,
      expandableStrings,
      filterPlaceHolder,
      object,
      renderRow,
      renderValue,
      sectionNames,
    } = this.props;

    return (
      div({ className: "properties-view" },
        this.shouldRenderSearchBox(object) &&
          div({ className: "searchbox-section" },
            SearchBox({
              delay: FILTER_SEARCH_DELAY,
              type: "filter",
              onChange: this.updateFilterText,
              placeholder: filterPlaceHolder,
            }),
          ),
        div({ className: "tree-container" },
          TreeView({
            object,
            columns: [{
              id: "value",
              width: "100%",
            }],
            decorator: decorator || {
              getRowClass: (rowObject) => this.getRowClass(rowObject, sectionNames),
            },
            enableInput,
            expandableStrings,
            useQuotes: false,
            expandedNodes: TreeViewClass.getExpandedNodes(
              object,
              {maxLevel: AUTO_EXPAND_MAX_LEVEL, maxNodes: AUTO_EXPAND_MAX_NODES}
            ),
            onFilter: (props) => this.onFilter(props, sectionNames),
            renderRow: renderRow || this.renderRowWithEditor,
            renderValue: renderValue || this.renderValueWithRep,
          }),
        ),
      )
    );
  }
});

module.exports = PropertiesView;
