/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global EVENTS */

// React & Redux
const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const TreeView = createFactory(
  require("resource://devtools/client/shared/components/tree/TreeView.js")
);
// Reps
const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/index.js");

const {
  fetchChildren,
} = require("resource://devtools/client/accessibility/actions/accessibles.js");

const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");
const {
  isFiltered,
} = require("resource://devtools/client/accessibility/utils/audit.js");
const AccessibilityRow = createFactory(
  require("resource://devtools/client/accessibility/components/AccessibilityRow.js")
);
const AccessibilityRowValue = createFactory(
  require("resource://devtools/client/accessibility/components/AccessibilityRowValue.js")
);
const {
  Provider,
} = require("resource://devtools/client/accessibility/provider.js");

const {
  scrollIntoView,
} = require("resource://devtools/client/shared/scroll.js");

/**
 * Renders Accessibility panel tree.
 */
class AccessibilityTree extends Component {
  static get propTypes() {
    return {
      toolboxDoc: PropTypes.object.isRequired,
      dispatch: PropTypes.func.isRequired,
      accessibles: PropTypes.object,
      expanded: PropTypes.object,
      selected: PropTypes.string,
      highlighted: PropTypes.object,
      filtered: PropTypes.bool,
      getAccessibilityTreeRoot: PropTypes.func.isRequired,
      startListeningForAccessibilityEvents: PropTypes.func.isRequired,
      stopListeningForAccessibilityEvents: PropTypes.func.isRequired,
      highlightAccessible: PropTypes.func.isRequired,
      unhighlightAccessible: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onNameChange = this.onNameChange.bind(this);
    this.onReorder = this.onReorder.bind(this);
    this.onTextChange = this.onTextChange.bind(this);
    this.renderValue = this.renderValue.bind(this);
    this.scrollSelectedRowIntoView = this.scrollSelectedRowIntoView.bind(this);
  }

  /**
   * Add accessibility event listeners that affect tree rendering and updates.
   */
  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillMount() {
    this.props.startListeningForAccessibilityEvents({
      reorder: this.onReorder,
      "name-change": this.onNameChange,
      "text-change": this.onTextChange,
    });
    window.on(
      EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED,
      this.scrollSelectedRowIntoView
    );
    return null;
  }

  componentDidUpdate(prevProps) {
    // When filtering is toggled, make sure that the selected row remains in
    // view.
    if (this.props.filtered !== prevProps.filtered) {
      this.scrollSelectedRowIntoView();
    }

    window.emit(EVENTS.ACCESSIBILITY_INSPECTOR_UPDATED);
  }

  /**
   * Remove accessible event listeners.
   */
  componentWillUnmount() {
    this.props.stopListeningForAccessibilityEvents({
      reorder: this.onReorder,
      "name-change": this.onNameChange,
      "text-change": this.onTextChange,
    });

    window.off(
      EVENTS.NEW_ACCESSIBLE_FRONT_INSPECTED,
      this.scrollSelectedRowIntoView
    );
  }

  /**
   * Handle accessible reorder event. If the accessible is cached and rendered
   * within the accessibility tree, re-fetch its children and re-render the
   * corresponding subtree.
   * @param {Object} accessibleFront
   *        accessible front that had its subtree reordered.
   */
  onReorder(accessibleFront) {
    if (this.props.accessibles.has(accessibleFront.actorID)) {
      this.props.dispatch(fetchChildren(accessibleFront));
    }
  }

  scrollSelectedRowIntoView() {
    const { treeview } = this.refs;
    if (!treeview) {
      return;
    }

    const treeEl = treeview.treeRef.current;
    if (!treeEl) {
      return;
    }

    const selected = treeEl.ownerDocument.querySelector(
      ".treeTable .treeRow.selected"
    );
    if (selected) {
      scrollIntoView(selected, { center: true });
    }
  }

  /**
   * Handle accessible name change event. If the name of an accessible changes
   * and that accessible is cached and rendered within the accessibility tree,
   * re-fetch its parent's children and re-render the corresponding subtree.
   * @param {Object} accessibleFront
   *        accessible front that had its name changed.
   * @param {Object} parentFront
   *        optional parent accessible front. Note: if it parent is not
   *        present, we assume that the top level document's name has changed
   *        and use accessible walker as a parent.
   */
  onNameChange(accessibleFront, parentFront) {
    const { accessibles, dispatch } = this.props;
    const accessibleWalkerFront = accessibleFront.getParent();
    parentFront = parentFront || accessibleWalkerFront;

    if (
      accessibles.has(accessibleFront.actorID) ||
      accessibles.has(parentFront.actorID)
    ) {
      dispatch(fetchChildren(parentFront));
    }
  }

  /**
   * Handle accessible text change (change/insert/remove) event. If the text of
   * an accessible changes and that accessible is cached and rendered within the
   * accessibility tree, re-fetch its children and re-render the corresponding
   * subtree.
   * @param  {Object} accessibleFront
   *         accessible front that had its child text changed.
   */
  onTextChange(accessibleFront) {
    const { accessibles, dispatch } = this.props;
    if (accessibles.has(accessibleFront.actorID)) {
      dispatch(fetchChildren(accessibleFront));
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
      toolboxDoc,
      filtered,
      getAccessibilityTreeRoot,
      highlightAccessible,
      unhighlightAccessible,
    } = this.props;

    const renderRow = rowProps => {
      const { object } = rowProps.member;
      const highlighted = object === highlightedItem;
      return AccessibilityRow(
        Object.assign({}, rowProps, {
          toolboxDoc,
          highlighted,
          decorator: {
            getRowClass() {
              return highlighted ? ["highlighted"] : [];
            },
          },
          highlightAccessible,
          unhighlightAccessible,
        })
      );
    };
    const className = filtered ? "filtered" : undefined;

    return TreeView({
      ref: "treeview",
      object: getAccessibilityTreeRoot(),
      mode: MODE.SHORT,
      provider: new Provider(accessibles, filtered, dispatch),
      columns,
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

        return true;
      },
      onContextMenuTree(e) {
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
  ui: { expanded, selected, highlighted },
  audit: { filters },
}) => ({
  accessibles,
  expanded,
  selected,
  highlighted,
  filtered: isFiltered(filters),
});
// Exports from this module
module.exports = connect(mapStateToProps)(AccessibilityTree);
