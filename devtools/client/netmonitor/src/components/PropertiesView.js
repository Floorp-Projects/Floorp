/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable react/prop-types */

"use strict";

const Services = require("Services");
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
const {
  FILTER_SEARCH_DELAY,
} = require("devtools/client/netmonitor/src/constants");

// Components
const TreeViewClass = require("devtools/client/shared/components/tree/TreeView");
const PropertiesViewContextMenu = require("devtools/client/netmonitor/src/widgets/PropertiesViewContextMenu");
const JSONPreview = createFactory(
  require("devtools/client/netmonitor/src/components/JSONPreview")
);

loader.lazyGetter(this, "SearchBox", function() {
  return createFactory(require("devtools/client/shared/components/SearchBox"));
});
loader.lazyGetter(this, "TreeRow", function() {
  return createFactory(
    require("devtools/client/shared/components/tree/TreeRow")
  );
});
loader.lazyGetter(this, "SourceEditor", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/SourceEditor")
  );
});
loader.lazyGetter(this, "HTMLPreview", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/previews/HtmlPreview")
  );
});

// Constants
const {
  AUTO_EXPAND_MAX_LEVEL,
  AUTO_EXPAND_MAX_NODES,
} = require("devtools/client/netmonitor/src/constants");

const { div, tr, td, pre } = dom;
const EDITOR_CONFIG_ID = "EDITOR_CONFIG";
const HTML_PREVIEW_ID = "HTML_PREVIEW";

/**
 * Properties View component
 * A scrollable tree view component which provides some useful features for
 * representing object properties.
 *
 * Search filter - Set enableFilter to enable / disable SearchBox feature.
 * Tree view - Default enabled.
 * Source editor - Enable by specifying object level 1 property name to EDITOR_CONFIG_ID.
 * HTML preview - Enable by specifying object level 1 property name to HTML_PREVIEW_ID.
 * Rep - Default enabled.
 */
class PropertiesView extends Component {
  static get propTypes() {
    return {
      object: PropTypes.object,
      provider: PropTypes.object,
      enableInput: PropTypes.bool,
      expandableStrings: PropTypes.bool,
      expandedNodes: PropTypes.object,
      filterPlaceHolder: PropTypes.string,
      sectionNames: PropTypes.array,
      openLink: PropTypes.func,
      cropLimit: PropTypes.number,
      targetSearchResult: PropTypes.object,
      resetTargetSearchResult: PropTypes.func,
      useQuotes: PropTypes.bool,
    };
  }

  static get defaultProps() {
    return {
      enableInput: true,
      enableFilter: true,
      expandableStrings: false,
      filterPlaceHolder: "",
      sectionNames: [],
      cropLimit: 1024,
      useQuotes: false,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      filterText: "",
    };

    this.getRowClass = this.getRowClass.bind(this);
    this.onFilter = this.onFilter.bind(this);
    this.renderRowWithExtras = this.renderRowWithExtras.bind(this);
    this.shouldRenderSearchBox = this.shouldRenderSearchBox.bind(this);
    this.updateFilterText = this.updateFilterText.bind(this);
  }

  /**
   * Update only if:
   * 1) The rendered object has changed
   * 2) The user selected another search result target.
   * 3) Internal state changes
   */
  shouldComponentUpdate(nextProps, nextState) {
    return (
      this.state !== nextState ||
      this.props.object !== nextProps.object ||
      (this.props.targetSearchResult !== nextProps.targetSearchResult &&
        nextProps.targetSearchResult !== null)
    );
  }

  getRowClass(object, sectionNames) {
    return sectionNames.includes(object.name) ? "tree-section" : "";
  }

  onFilter(object, whiteList) {
    const { name, value } = object;
    const filterText = this.state.filterText;

    if (!filterText || whiteList.includes(name)) {
      return true;
    }

    const jsonString = JSON.stringify({ [name]: value }).toLowerCase();
    return jsonString.includes(filterText.toLowerCase());
  }

  renderRowWithExtras(props) {
    const { level, name, value, path } = props.member;

    // Display source editor when specifying to EDITOR_CONFIG_ID along with config
    if (level === 1 && name === EDITOR_CONFIG_ID) {
      return tr(
        { key: EDITOR_CONFIG_ID, className: "editor-row-container" },
        td({ colSpan: 2 }, this.renderResponseText(props))
      );
    }

    // Similar to the source editor, display a preview when specifying HTML_PREVIEW_ID
    if (level === 1 && name === HTML_PREVIEW_ID) {
      return tr(
        {
          key: HTML_PREVIEW_ID,
          className: "response-preview-container",
        },
        td({ colSpan: 2 }, HTMLPreview(value))
      );
    }

    // Skip for editor config and HTML previews
    if (
      (path.includes(EDITOR_CONFIG_ID) || path.includes(HTML_PREVIEW_ID)) &&
      level >= 1
    ) {
      return null;
    }

    return TreeRow(props);
  }

  renderResponseText(props) {
    const { value } = props.member;
    let responseTextComponent = SourceEditor(value);

    // To prevent performance issues, switch from SourceEditor to pre()
    // if response size is greater than specified limit.
    const limit = Services.prefs.getIntPref(
      "devtools.netmonitor.response.ui.limit"
    );

    // Scroll to specified line if the user clicks on search results.
    const scrollToLine = element => {
      const { targetSearchResult, resetTargetSearchResult } = this.props;

      // The following code is responsible for scrolling given line
      // to visible view-port.
      // It gets the <div> child element representing the target
      // line (by index) and uses `scrollIntoView` API to make sure
      // it's visible to the user.
      if (element && targetSearchResult && targetSearchResult.line) {
        const child = element.children[targetSearchResult.line - 1];
        if (child) {
          const range = document.createRange();
          range.selectNode(child);
          document.getSelection().addRange(range);
          child.scrollIntoView({ block: "center" });
        }
        resetTargetSearchResult();
      }
    };

    if (value?.text && value.text.length > limit) {
      responseTextComponent = div(
        { className: "responseTextContainer" },
        pre(
          { ref: element => scrollToLine(element) },
          value.text.split(/\r\n|\r|\n/).map((line, index) => {
            return div({ key: index }, line);
          })
        )
      );
    }

    return responseTextComponent;
  }

  onContextMenuRow(member, evt) {
    evt.preventDefault();

    const { object } = member;

    // Select the right clicked row
    this.selectRow(evt.currentTarget);

    // if data exists and can be copied, then show the contextmenu
    if (typeof object === "object") {
      if (!this.contextMenu) {
        this.contextMenu = new PropertiesViewContextMenu({});
      }
      this.contextMenu.open(evt, { member, object: this.props.object });
    }
  }

  sectionIsSearchable(object, section) {
    return !(
      object[section][EDITOR_CONFIG_ID] || object[section][HTML_PREVIEW_ID]
    );
  }

  shouldRenderSearchBox(object) {
    return (
      this.props.enableFilter &&
      object &&
      Object.keys(object).filter(section =>
        this.sectionIsSearchable(object, section)
      ).length > 0
    );
  }

  updateFilterText(filterText) {
    this.setState({
      filterText,
    });
  }

  render() {
    const {
      decorator,
      enableInput,
      expandableStrings,
      expandedNodes,
      filterPlaceHolder,
      object,
      renderRow,
      renderValue,
      sectionNames,
      openLink,
      provider,
      selected,
      useQuotes,
    } = this.props;

    return div(
      { className: "properties-view" },
      this.shouldRenderSearchBox(object) &&
        div(
          { className: "devtools-toolbar devtools-input-toolbar" },
          SearchBox({
            delay: FILTER_SEARCH_DELAY,
            type: "filter",
            onChange: this.updateFilterText,
            placeholder: filterPlaceHolder,
          })
        ),
      JSONPreview({
        object,
        provider,
        columns: [
          {
            id: "value",
            width: "100%",
          },
        ],
        decorator: decorator || {
          getRowClass: rowObject => this.getRowClass(rowObject, sectionNames),
        },
        enableInput,
        expandableStrings,
        useQuotes,
        expandedNodes:
          expandedNodes ||
          TreeViewClass.getExpandedNodes(object, {
            maxLevel: AUTO_EXPAND_MAX_LEVEL,
            maxNodes: AUTO_EXPAND_MAX_NODES,
          }),
        onFilter: props => this.onFilter(props, sectionNames),
        renderRow: renderRow || this.renderRowWithExtras,
        renderValue,
        openLink,
        onContextMenuRow: this.onContextMenuRow,
        selected,
      })
    );
  }
}

module.exports = connect(null, dispatch => ({
  resetTargetSearchResult: () => dispatch(setTargetSearchResult(null)),
}))(PropertiesView);
