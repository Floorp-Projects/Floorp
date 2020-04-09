/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable react/prop-types */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const {
  setTargetSearchResult,
} = require("devtools/client/netmonitor/src/actions/search");

// Components
const TreeViewClass = require("devtools/client/shared/components/tree/TreeView");
const TreeView = createFactory(TreeViewClass);
const PropertiesViewContextMenu = require("devtools/client/netmonitor/src/widgets/PropertiesViewContextMenu");

loader.lazyGetter(this, "Rep", function() {
  return require("devtools/client/shared/components/reps/reps").REPS.Rep;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});

// Constants
const {
  AUTO_EXPAND_MAX_LEVEL,
  AUTO_EXPAND_MAX_NODES,
} = require("devtools/client/netmonitor/src/constants");

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
      filterText: PropTypes.string,
      cropLimit: PropTypes.number,
      targetSearchResult: PropTypes.object,
      resetTargetSearchResult: PropTypes.func,
      selectPath: PropTypes.func,
      mode: PropTypes.symbol,
    };
  }

  static get defaultProps() {
    return {
      enableInput: true,
      enableFilter: true,
      expandableStrings: false,
      cropLimit: 1024,
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
    const {
      targetSearchResult,
      resetTargetSearchResult,
      selectPath,
    } = this.props;
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
        this.contextMenu = new PropertiesViewContextMenu({});
      }
      this.contextMenu.open(evt, { member, object: this.props.object });
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
      decorator,
      enableInput,
      expandableStrings,
      expandedNodes,
      object,
      renderRow,
      renderValue,
      provider,
      targetSearchResult,
      selectPath,
      cropLimit,
    } = this.props;

    return div(
      { className: "properties-view" },
      div(
        { className: "tree-container" },
        TreeView({
          ref: () => this.scrollSelectedIntoView(),
          object,
          provider,
          columns: [{ id: "value", width: "100%" }],
          decorator,
          enableInput,
          expandableStrings,
          useQuotes: true,
          expandedNodes:
            expandedNodes ||
            TreeViewClass.getExpandedNodes(object, {
              maxLevel: AUTO_EXPAND_MAX_LEVEL,
              maxNodes: AUTO_EXPAND_MAX_NODES,
            }),
          onFilter: props => this.onFilter(props),
          renderRow,
          renderValue: renderValue || this.renderValueWithRep,
          onContextMenuRow: this.onContextMenuRow,
          selected:
            typeof selectPath == "function"
              ? selectPath(targetSearchResult)
              : this.getSelectedPath(targetSearchResult),
          cropLimit,
        })
      )
    );
  }
}

module.exports = connect(null, dispatch => ({
  resetTargetSearchResult: () => dispatch(setTargetSearchResult(null)),
}))(PropertiesView);
