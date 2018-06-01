/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global EVENTS */

// React & Redux
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const TreeView = createFactory(require("devtools/client/shared/components/tree/TreeView"));
// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const Rep = createFactory(REPS.Rep);
const Grip = REPS.Grip;

const { fetchChildren } = require("../actions/accessibles");

const { L10N } = require("../utils/l10n");
const AccessibilityRow = createFactory(require("./AccessibilityRow"));
const { Provider } = require("../provider");

/**
 * Renders Accessibility panel tree.
 */
class AccessibilityTree extends Component {
  static get propTypes() {
    return {
      walker: PropTypes.object,
      dispatch: PropTypes.func.isRequired,
      accessibles: PropTypes.object,
      expanded: PropTypes.object,
      selected: PropTypes.string,
      highlighted: PropTypes.object
    };
  }

  constructor(props) {
    super(props);

    this.onNameChange = this.onNameChange.bind(this);
    this.onReorder = this.onReorder.bind(this);
    this.onTextChange = this.onTextChange.bind(this);
  }

  /**
   * Add accessibility walker front event listeners that affect tree rendering
   * and updates.
   */
  componentWillMount() {
    const { walker } = this.props;
    walker.on("reorder", this.onReorder);
    walker.on("name-change", this.onNameChange);
    walker.on("text-change", this.onTextChange);
    return null;
  }

  componentDidUpdate() {
    window.emit(EVENTS.ACCESSIBILITY_INSPECTOR_UPDATED);
  }

  /**
   * Remove accessible walker front event listeners.
   */
  componentWillUnmount() {
    const { walker } = this.props;
    walker.off("reorder", this.onReorder);
    walker.off("name-change", this.onNameChange);
    walker.off("text-change", this.onTextChange);
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
    const { accessibles, walker, dispatch } = this.props;
    parent = parent || walker;

    if (accessibles.has(accessible.actorID) ||
        accessibles.has(parent.actorID)) {
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

  /**
   * Render Accessibility panel content
   */
  render() {
    const columns = [{
      "id": "default",
      "title": L10N.getStr("accessibility.role")
    }, {
      "id": "value",
      "title": L10N.getStr("accessibility.name")
    }];

    const {
      accessibles,
      dispatch,
      expanded,
      selected,
      highlighted: highlightedItem,
      walker
    } = this.props;

    const renderValue = props => {
      return Rep(Object.assign({}, props, {
        defaultRep: Grip,
        cropLimit: 50,
      }));
    };

    const renderRow = rowProps => {
      const { object } = rowProps.member;
      const highlighted = object === highlightedItem;
      return AccessibilityRow(Object.assign({}, rowProps, {
        walker,
        highlighted,
        decorator: {
          getRowClass: function() {
            return highlighted ? ["highlighted"] : [];
          }
        }
      }));
    };

    return (
      TreeView({
        object: walker,
        mode: MODE.SHORT,
        provider: new Provider(accessibles, dispatch),
        columns: columns,
        renderValue,
        renderRow,
        label: L10N.getStr("accessibility.treeName"),
        header: true,
        expandedNodes: expanded,
        selected,
        onClickRow(nodePath, event) {
          event.stopPropagation();
          if (event.target.classList.contains("theme-twisty")) {
            this.toggle(nodePath);
          }
          this.selectRow(event.currentTarget);
        }
      })
    );
  }
}

const mapStateToProps = ({ accessibles, ui }) => ({
  accessibles,
  expanded: ui.expanded,
  selected: ui.selected,
  highlighted: ui.highlighted
});
// Exports from this module
module.exports = connect(mapStateToProps)(AccessibilityTree);
