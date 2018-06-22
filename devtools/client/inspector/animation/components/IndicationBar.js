/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const { PureComponent } = require("devtools/client/shared/vendor/react");

/**
 * This class provides the shape of indication bar such as scrubber and progress bar.
 * Also, make the bar to move to correct position even resizing animation inspector.
 */
class IndicationBar extends PureComponent {
  static get propTypes() {
    return {
      className: PropTypes.string.isRequired,
      position: PropTypes.number.isRequired,
      sidebarWidth: PropTypes.number.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // offset of the position for this bar
      offset: 0,
    };
  }

  componentDidMount() {
    this.updateState(this.props);
  }

  componentWillReceiveProps(nextProps) {
    this.updateState(nextProps);
  }

  updateState(props) {
    const { position } = props;

    const parentEl = findDOMNode(this).parentNode;
    const offset = parentEl.offsetWidth * position;

    this.setState({ offset });
  }

  render() {
    const { className } = this.props;
    const { offset } = this.state;

    return dom.div(
      {
        className: `indication-bar ${ className }`,
        style: {
          marginInlineStart: offset + "px",
        },
      }
    );
  }
}

const mapStateToProps = state => {
  return {
    sidebarWidth: state.animations.sidebarSize ? state.animations.sidebarSize.width : 0
  };
};

module.exports = connect(mapStateToProps)(IndicationBar);
