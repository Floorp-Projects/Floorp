/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable react/prop-types */

"use strict";

const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/redux/visibility-handler-connect.js");
const {
  setTargetSearchResult,
} = require("resource://devtools/client/netmonitor/src/actions/search.js");

// Components
const TreeViewClass = require("resource://devtools/client/shared/components/tree/TreeView.js");
const TreeView = createFactory(TreeViewClass);
const PropertiesViewContextMenu = require("resource://devtools/client/netmonitor/src/widgets/PropertiesViewContextMenu.js");

loader.lazyGetter(this, "Rep", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .REPS.Rep;
});
loader.lazyGetter(this, "MODE", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .MODE;
});

// Constants
const {
  AUTO_EXPAND_MAX_LEVEL,
  AUTO_EXPAND_MAX_NODES,
} = require("resource://devtools/client/netmonitor/src/constants.js");

const { div } = dom;

/**
 * Properties View component
 * A scrollable tree view component which provides some useful features for
 * representing object properties.
 *
 * Tree view
 * Rep
 */
class PropertiesView extends Component {
  static get propTypes() {
    return {
      object: PropTypes.oneOfType([PropTypes.object, PropTypes.array]),
      provider: PropTypes.object,
      enableInput: PropTypes.bool,
      expandableStrings: PropTypes.bool,
      expandedNodes: PropTypes.object,
      useBaseTreeViewExpand: PropTypes.bool,
      filterText: PropTypes.string,
      cropLimit: PropTypes.number,
      targetSearchResult: PropTypes.object,
      resetTargetSearchResult: PropTypes.func,
      selectPath: PropTypes.func,
      mode: PropTypes.symbol,
      defaultSelectFirstNode: PropTypes.bool,
      useQuotes: PropTypes.bool,
      onClickRow: PropTypes.func,
      contextMenuFormatters: PropTypes.object,
    };
  }

  static get defaultProps() {
    return {
      enableInput: true,
      enableFilter: true,
      expandableStrings: false,
      cropLimit: 1024,
      useQuotes: true,
      contextMenuFormatters: {},
      useBaseTreeViewExpand: false,
    };
  }

  constructor(props) {
    super(props);
    this.onFilter = this.onFilter.bind(this);
    this.renderValueWithRep = this.renderValueWithRep.bind(this);
    this.getSelectedPath = this.getSelectedPath.bind(this);
  }

  /**
   * Update only if:
   * 1) The rendered object has changed
   * 2) The filter text has changed
   * 3) The user selected another search result target.
   */
  shouldComponentUpdate(nextProps) {
    return (
      this.props.object !== nextProps.object ||
      this.props.filterText !== nextProps.filterText ||
      (this.props.targetSearchResult !== nextProps.targetSearchResult &&
        nextProps.targetSearchResult !== null)
    );
  }

  onFilter(props) {
    const { name, value } = props;
    const { filterText } = this.props;

    if (!filterText) {
      return true;
    }

    const jsonString = JSON.stringify({ [name]: value }).toLowerCase();
    return jsonString.includes(filterText.toLowerCase());
  }

  getSelectedPath(targetSearchResult) {
    if (!targetSearchResult) {
      return null;
    }

    return `/${targetSearchResult.label}`;
  }

  /**
   * If target is selected, let's scroll the content
   * so the property is visible. This is used for search result navigation,
   * which happens when the user clicks on a search result.
   */
  scrollSelectedIntoView() {
    const { targetSearchResult, resetTargetSearchResult, selectPath } =
      this.props;
    if (!targetSearchResult) {
      return;
    }

    const path =
      typeof selectPath == "function"
        ? selectPath(targetSearchResult)
        : this.getSelectedPath(targetSearchResult);
    const element = document.getElementById(path);
    if (element) {
      element.scrollIntoView({ block: "center" });
    }

    resetTargetSearchResult();
  }

  onContextMenuRow(member, evt) {
    evt.preventDefault();

    const { object } = member;

    // Select the right clicked row
    this.selectRow({ props: { member } });

    // if data exists and can be copied, then show the contextmenu
    if (typeof object === "object") {
      if (!this.contextMenu) {
        this.contextMenu = new PropertiesViewContextMenu({
          customFormatters: this.props.contextMenuFormatters,
        });
      }
      this.contextMenu.open(evt, window.getSelection(), {
        member,
        object: this.props.object,
      });
    }
  }

  renderValueWithRep(props) {
    const { member } = props;

    /* Hide strings with following conditions
     * - the `value` object has a `value` property (only happens in Cookies panel)
     */
    if (typeof member.value === "object" && member.value?.value) {
      return null;
    }

    return Rep(
      Object.assign(props, {
        // FIXME: A workaround for the issue in StringRep
        // Force StringRep to crop the text every time
        member: Object.assign({}, member, { open: false }),
        mode: this.props.mode || MODE.TINY,
        cropLimit: this.props.cropLimit,
        noGrip: true,
      })
    );
  }

  render() {
    const {
      useBaseTreeViewExpand,
      expandedNodes,
      object,
      renderValue,
      targetSearchResult,
      selectPath,
    } = this.props;

    let currentExpandedNodes;
    // In the TreeView, when the component is re-rendered
    // the state of `expandedNodes` is persisted by default
    // e.g. when you open a node and filter the properties list,
    // the node remains open.
    // We have the prop `useBaseTreeViewExpand` to flag when we want to use
    // this functionality or not.
    if (!useBaseTreeViewExpand) {
      currentExpandedNodes =
        expandedNodes ||
        TreeViewClass.getExpandedNodes(object, {
          maxLevel: AUTO_EXPAND_MAX_LEVEL,
          maxNodes: AUTO_EXPAND_MAX_NODES,
        });
    }
    return div(
      { className: "properties-view" },
      div(
        { className: "tree-container" },
        TreeView({
          ...this.props,
          ref: () => this.scrollSelectedIntoView(),
          columns: [{ id: "value", width: "100%" }],

          expandedNodes: currentExpandedNodes,

          onFilter: props => this.onFilter(props),
          renderValue: renderValue || this.renderValueWithRep,
          onContextMenuRow: this.onContextMenuRow,
          selected:
            typeof selectPath == "function"
              ? selectPath(targetSearchResult)
              : this.getSelectedPath(targetSearchResult),
        })
      )
    );
  }
}

module.exports = connect(null, dispatch => ({
  resetTargetSearchResult: () => dispatch(setTargetSearchResult(null)),
}))(PropertiesView);
