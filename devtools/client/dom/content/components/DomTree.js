/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global DomProvider */

"use strict";

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
const { REPS, MODE } = require("devtools/client/shared/components/reps/index");
const { Rep } = REPS;

const Grip = REPS.Grip;
// DOM Panel
const { GripProvider } = require("devtools/client/dom/content/grip-provider");

const { DomDecorator } = require("devtools/client/dom/content/dom-decorator");

/**
 * Renders DOM panel tree.
 */
class DomTree extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      filter: PropTypes.string,
      grips: PropTypes.object,
      object: PropTypes.any,
      openLink: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);
    this.onFilter = this.onFilter.bind(this);
  }

  /**
   * Filter DOM properties. Return true if the object
   * should be visible in the tree.
   */
  onFilter(object) {
    if (!this.props.filter) {
      return true;
    }

    return object.name && object.name.indexOf(this.props.filter) > -1;
  }

  /**
   * Render DOM panel content
   */
  render() {
    const { dispatch, grips, object, openLink } = this.props;

    const columns = [
      {
        id: "value",
      },
    ];

    let onDOMNodeMouseOver;
    let onDOMNodeMouseOut;
    let onInspectIconClick;
    const toolbox = DomProvider.getToolbox();
    if (toolbox) {
      const highlighter = toolbox.getHighlighter();
      onDOMNodeMouseOver = async (grip, options = {}) => {
        return highlighter.highlight(grip, options);
      };
      onDOMNodeMouseOut = async () => {
        return highlighter.unhighlight();
      };
      onInspectIconClick = async grip => {
        return toolbox.viewElementInInspector(grip, "inspect_dom");
      };
    }

    // This is the integration point with Reps. The DomTree is using
    // Reps to render all values. The code also specifies default rep
    // used for data types that don't have its own specific template.
    const renderValue = props => {
      const repProps = Object.assign({}, props, {
        onDOMNodeMouseOver,
        onDOMNodeMouseOut,
        onInspectIconClick,
        defaultRep: Grip,
        cropLimit: 50,
      });

      // Object can be an objectFront, while Rep always expect grips.
      if (props?.object?.getGrip) {
        repProps.object = props.object.getGrip();
      }

      return Rep(repProps);
    };

    return TreeView({
      columns,
      decorator: new DomDecorator(),
      mode: MODE.SHORT,
      object,
      onFilter: this.onFilter,
      openLink,
      provider: new GripProvider(grips, dispatch),
      renderValue,
    });
  }
}

const mapStateToProps = state => {
  return {
    grips: state.grips,
    filter: state.filter,
  };
};

// Exports from this module
module.exports = connect(mapStateToProps)(DomTree);
