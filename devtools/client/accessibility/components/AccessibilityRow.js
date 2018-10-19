/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gToolbox, EVENTS */

// React & Redux
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const TreeRow = require("devtools/client/shared/components/tree/TreeRow");

// Utils
const {flashElementOn, flashElementOff} =
  require("devtools/client/inspector/markup/utils");
const { VALUE_FLASHING_DURATION, VALUE_HIGHLIGHT_DURATION } = require("../constants");

// Actions
const { updateDetails } = require("../actions/details");
const { unhighlight } = require("../actions/accessibles");

class HighlightableTreeRowClass extends TreeRow {
  shouldComponentUpdate(nextProps) {
    const props = ["name", "open", "value", "loading", "selected", "hasChildren"];

    for (const p of props) {
      if (nextProps.member[p] !== this.props.member[p]) {
        return true;
      }
    }

    if (nextProps.highlighted !== this.props.highlighted) {
      return true;
    }

    return false;
  }
}

const HighlightableTreeRow = createFactory(HighlightableTreeRowClass);

// Component that expands TreeView's own TreeRow and is responsible for
// rendering an accessible object.
class AccessibilityRow extends Component {
  static get propTypes() {
    return {
      ...TreeRow.propTypes,
      dispatch: PropTypes.func.isRequired,
      walker: PropTypes.object,
    };
  }

  componentDidMount() {
    const { selected, object } = this.props.member;
    if (selected) {
      this.updateAndScrollIntoViewIfNeeded();
      this.highlight(object, { duration: VALUE_HIGHLIGHT_DURATION });
    }

    if (this.props.highlighted) {
      this.scrollIntoView();
    }
  }

  /**
   * Update accessible object details that are going to be rendered inside the
   * accessible panel sidebar.
   */
  componentDidUpdate(prevProps) {
    const { selected, object } = this.props.member;
    // If row is selected, update corresponding accessible details.
    if (!prevProps.member.selected && selected) {
      this.updateAndScrollIntoViewIfNeeded();
      this.highlight(object, { duration: VALUE_HIGHLIGHT_DURATION });
    }

    if (this.props.highlighted) {
      this.scrollIntoView();
    }

    if (!selected && prevProps.member.value !== this.props.member.value) {
      this.flashValue();
    }
  }

  scrollIntoView() {
    const row = findDOMNode(this);
    row.scrollIntoView({ block: "center" });
  }

  updateAndScrollIntoViewIfNeeded() {
    const { dispatch, member, supports } = this.props;
    if (gToolbox) {
      dispatch(updateDetails(gToolbox.walker, member.object, supports));
    }

    this.scrollIntoView();
    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_SELECTED, member.object);
  }

  flashValue() {
    const row = findDOMNode(this);
    const value = row.querySelector(".objectBox");

    flashElementOn(value);
    if (this._flashMutationTimer) {
      clearTimeout(this._flashMutationTimer);
      this._flashMutationTimer = null;
    }
    this._flashMutationTimer = setTimeout(() => {
      flashElementOff(value);
    }, VALUE_FLASHING_DURATION);
  }

  highlight(accessible, options) {
    const { walker, dispatch } = this.props;
    dispatch(unhighlight());

    if (!accessible || !walker) {
      return;
    }

    walker.highlightAccessible(accessible, options).catch(error =>
      console.warn(error));
  }

  unhighlight() {
    const { walker, dispatch } = this.props;
    dispatch(unhighlight());

    if (!walker) {
      return;
    }

    walker.unhighlight().catch(error => console.warn(error));
  }

  /**
   * Render accessible row component.
   * @returns acecssible-row React component.
   */
  render() {
    const { object } = this.props.member;
    const props = Object.assign({}, this.props, {
      onMouseOver: () => this.highlight(object),
      onMouseOut: () => this.unhighlight(),
    });

    return (HighlightableTreeRow(props));
  }
}

const mapStateToProps = ({ ui }) => ({
  supports: ui.supports,
});

module.exports = connect(mapStateToProps)(AccessibilityRow);
