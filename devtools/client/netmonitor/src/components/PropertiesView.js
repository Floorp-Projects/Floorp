/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable react/prop-types */

"use strict";

const Services = require("Services");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { FILTER_SEARCH_DELAY } = require("../constants");

// Components
const TreeViewClass = require("devtools/client/shared/components/tree/TreeView");
const PropertiesViewContextMenu = require("../widgets/PropertiesViewContextMenu");
const TreeView = createFactory(TreeViewClass);

loader.lazyGetter(this, "SearchBox", function() {
  return createFactory(require("devtools/client/shared/components/SearchBox"));
});
loader.lazyGetter(this, "TreeRow", function() {
  return createFactory(require("devtools/client/shared/components/tree/TreeRow"));
});
loader.lazyGetter(this, "SourceEditor", function() {
  return createFactory(require("./SourceEditor"));
});
loader.lazyGetter(this, "HTMLPreview", function() {
  return createFactory(require("./HtmlPreview"));
});

loader.lazyGetter(this, "Rep", function() {
  return require("devtools/client/shared/components/reps/reps").REPS.Rep;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});

const { div, tr, td, pre } = dom;
const AUTO_EXPAND_MAX_LEVEL = 7;
const AUTO_EXPAND_MAX_NODES = 50;
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
      filterPlaceHolder: PropTypes.string,
      sectionNames: PropTypes.array,
      openLink: PropTypes.func,
      cropLimit: PropTypes.number,
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
    this.renderValueWithRep = this.renderValueWithRep.bind(this);
    this.shouldRenderSearchBox = this.shouldRenderSearchBox.bind(this);
    this.updateFilterText = this.updateFilterText.bind(this);
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

    // To prevent performance issues, switch from SourceEditor to pre()
    // if response size is greater than specified limit.
    let responseTextComponent = SourceEditor(value);
    const limit = Services.prefs.getIntPref("devtools.netmonitor.response.ui.limit");
    if (value && value.text && value.text.length > limit) {
      responseTextComponent =
        div({className: "responseTextContainer"},
          pre({}, value.text)
        );
    }

    // Display source editor when specifying to EDITOR_CONFIG_ID along with config
    if (level === 1 && name === EDITOR_CONFIG_ID) {
      return (
        tr({ key: EDITOR_CONFIG_ID, className: "editor-row-container" },
          td({ colSpan: 2 },
            responseTextComponent
          )
        )
      );
    }

    // Similar to the source editor, display a preview when specifying HTML_PREVIEW_ID
    if (level === 1 && name === HTML_PREVIEW_ID) {
      return (
        tr({ key: HTML_PREVIEW_ID, className: "response-preview-container" },
          td({ colSpan: 2 },
            HTMLPreview(value)
          )
        )
      );
    }

    // Skip for editor config and HTML previews
    if ((path.includes(EDITOR_CONFIG_ID) || path.includes(HTML_PREVIEW_ID))
      && level >= 1) {
      return null;
    }

    return TreeRow(props);
  }

  onContextMenuRow(member, evt) {
    evt.preventDefault();

    const { object } = member;

    // Select the right clicked row
    this.selectRow(evt.currentTarget);

    // if data exists and can be copied, then show the contextmenu
    if (typeof (object) === "object") {
      if (!this.contextMenu) {
        this.contextMenu = new PropertiesViewContextMenu({});
      }
      this.contextMenu.open(evt, { member, object: this.props.object });
    }
  }

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
      // Force StringRep to crop the text every time
      member: Object.assign({}, member, { open: false }),
      mode: MODE.TINY,
      cropLimit: this.props.cropLimit,
      noGrip: true,
    }));
  }

  sectionIsSearchable(object, section) {
    return !(object[section][EDITOR_CONFIG_ID] || object[section][HTML_PREVIEW_ID]);
  }

  shouldRenderSearchBox(object) {
    return this.props.enableFilter && object && Object.keys(object)
      .filter((section) => this.sectionIsSearchable(object, section)).length > 0;
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
      filterPlaceHolder,
      object,
      renderRow,
      renderValue,
      sectionNames,
      openLink,
      provider,
    } = this.props;

    return (
      div({ className: "properties-view" },
        this.shouldRenderSearchBox(object) &&
          div({ className: "devtools-toolbar devtools-input-toolbar" },
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
            provider,
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
            renderRow: renderRow || this.renderRowWithExtras,
            renderValue: renderValue || this.renderValueWithRep,
            openLink,
            onContextMenuRow: this.onContextMenuRow,
          }),
        ),
      )
    );
  }
}

module.exports = PropertiesView;
