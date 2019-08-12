/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global EVENTS */

// React & Redux
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const TreeView = createFactory(
  require("devtools/client/shared/components/tree/TreeView")
);
// Reps
const { MODE } = require("devtools/client/shared/components/reps/reps");

const { fetchChildren } = require("../actions/accessibles");

const { L10N } = require("../utils/l10n");
const { isFiltered } = require("../utils/audit");
const AccessibilityRow = createFactory(require("./AccessibilityRow"));
const AccessibilityRowValue = createFactory(require("./AccessibilityRowValue"));
const { Provider } = require("../provider");

const { scrollIntoView } = require("devtools/client/shared/scroll");

/**
 * Renders Accessibility panel tree.
 */
class AccessibilityTree extends Component {
  static get propTypes() {
    return {
      accessibilityWalker: PropTypes.object,
      getDOMWalker: PropTypes.func.isRequired,
      dispatch: PropTypes.func.isRequired,
      accessibles: PropTypes.object,
      expanded: PropTypes.object,
      selected: PropTypes.string,
      highlighted: PropTypes.object,
      supports: PropTypes.object,
      filtered: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    this.onNameChange = this.onNameChange.bind(this);
    this.onReorder = this.onReorder.bind(this);
    this.onTextChange = this.onTextChange.bind(this);
    this.renderValue = this.renderValue.bind(this);
  }

  /**
   * Add accessibility walker front event listeners that affect tree rendering
   * and updates.
   */
  componentWillMount() {
    const { accessibilityWalker } = this.props;
    accessibilityWalker.on("reorder", this.onReorder);
    accessibilityWalker.on("name-change", this.onNameChange);
    accessibilityWalker.on("text-change", this.onTextChange);
    return null;
  }

  componentDidUpdate(prevProps) {
    // When filtering is toggled, make sure that the selected row remains in
    // view.
    if (this.props.filtered !== prevProps.filtered) {
      const selected = document.querySelector(".treeTable .treeRow.selected");
      if (selected) {
        scrollIntoView(selected, { center: true });
      }
    }

    window.emit(EVENTS.ACCESSIBILITY_INSPECTOR_UPDATED);
  }

  /**
   * Remove accessible walker front event listeners.
   */
  componentWillUnmount() {
    const { accessibilityWalker } = this.props;
    accessibilityWalker.off("reorder", this.onReorder);
    accessibilityWalker.off("name-change", this.onNameChange);
    accessibilityWalker.off("text-change", this.onTextChange);
  }

  /**
   * Handle accessible reorder event. If the accessible is cached and rendered
   * within the accessibility tree, re-fetch its children and re-render the
   * corresponding subtree.
   * @param {Object} accessible accessible object that had its subtree
   *                            reordered.
   */
  onReorder(accessible) {
    if (this.props.accessibles.has(accessible.actorID)) {
      this.props.dispatch(fetchChildren(accessible));
    }
  }

  /**
   * Handle accessible name change event. If the name of an accessible changes
   * and that accessible is cached and rendered within the accessibility tree,
   * re-fetch its parent's children and re-render the corresponding subtree.
   * @param {Object} accessible accessible object that had its name changed.
   * @param {Object} parent     optional parent accessible object. Note: if it
   *                            parent is not present, we assume that the top
   *                            level document's name has changed and use
   *                            accessible walker as a parent.
   */
  onNameChange(accessible, parent) {
    const { accessibles, accessibilityWalker, dispatch } = this.props;
    parent = parent || accessibilityWalker;

    if (
      accessibles.has(accessible.actorID) ||
      accessibles.has(parent.actorID)
    ) {
      dispatch(fetchChildren(parent));
    }
  }

  /**
   * Handle accessible text change (change/insert/remove) event. If the text of
   * an accessible changes and that accessible is cached and rendered within the
   * accessibility tree, re-fetch its children and re-render the corresponding
   * subtree.
   * @param  {Object} accessible  accessible object that had its child text
   *                              changed.
   */
  onTextChange(accessible) {
    const { accessibles, dispatch } = this.props;
    if (accessibles.has(accessible.actorID)) {
      dispatch(fetchChildren(accessible));
    }
  }

  renderValue(props) {
    return AccessibilityRowValue(props);
  }

  /**
   * Render Accessibility panel content
   */
  render() {
    const columns = [
      {
        id: "default",
        title: L10N.getStr("accessibility.role"),
      },
      {
        id: "value",
        title: L10N.getStr("accessibility.name"),
      },
    ];

    const {
      accessibles,
      dispatch,
      expanded,
      selected,
      highlighted: highlightedItem,
      supports,
      accessibilityWalker,
      getDOMWalker,
      filtered,
    } = this.props;

    // Historically, the first context menu item is snapshot function and it is available
    // for all accessible object.
    const hasContextMenu = supports.snapshot;

    const renderRow = rowProps => {
      const { object } = rowProps.member;
      const highlighted = object === highlightedItem;
      return AccessibilityRow(
        Object.assign({}, rowProps, {
          accessibilityWalker,
          getDOMWalker,
          hasContextMenu,
          highlighted,
          decorator: {
            getRowClass: function() {
              return highlighted ? ["highlighted"] : [];
            },
          },
        })
      );
    };
    const className = filtered ? "filtered" : undefined;

    return TreeView({
      object: accessibilityWalker,
      mode: MODE.SHORT,
      provider: new Provider(accessibles, filtered, dispatch),
      columns: columns,
      className,
      renderValue: this.renderValue,
      renderRow,
      label: L10N.getStr("accessibility.treeName"),
      header: true,
      expandedNodes: expanded,
      selected,
      onClickRow(nodePath, event) {
        if (event.target.classList.contains("theme-twisty")) {
          this.toggle(nodePath);
        }

        this.selectRow(
          this.rows.find(row => row.props.member.path === nodePath),
          { preventAutoScroll: true }
        );
      },
      onContextMenuTree:
        hasContextMenu &&
        function(e) {
          // If context menu event is triggered on (or bubbled to) the TreeView, it was
          // done via keyboard. Open context menu for currently selected row.
          let row = this.getSelectedRow();
          if (!row) {
            return;
          }

          row = row.getWrappedInstance();
          row.onContextMenu(e);
        },
    });
  }
}

const mapStateToProps = ({
  accessibles,
  ui: { expanded, selected, supports, highlighted },
  audit: { filters },
}) => ({
  accessibles,
  expanded,
  selected,
  supports,
  highlighted,
  filtered: isFiltered(filters),
});
// Exports from this module
module.exports = connect(mapStateToProps)(AccessibilityTree);
