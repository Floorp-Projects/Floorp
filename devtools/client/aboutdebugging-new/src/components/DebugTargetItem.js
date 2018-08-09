/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Actions = require("../actions/index");

/**
 * This component displays debug target.
 */
class DebugTargetItem extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      icon: PropTypes.string.isRequired,
      id: PropTypes.string.isRequired,
      name: PropTypes.string.isRequired,
      type: PropTypes.string.isRequired,
      url: PropTypes.string.isRequired,
    };
  }

  inspect() {
    const { dispatch, type, id } = this.props;
    dispatch(Actions.inspectDebugTarget(type, id));
  }

  renderIcon() {
    return dom.img({
      className: "debug-target-item__icon",
      src: this.props.icon,
    });
  }

  renderInfo() {
    return dom.div(
      {
        className: "debug-target-item__info",
      },
      dom.div(
        {
          className: "debug-target-item__info__name ellipsis-text",
          title: this.props.name,
        },
        this.props.name
      ),
      dom.div(
        {
          className: "debug-target-item__info__url ellipsis-text",
        },
        this.props.url
      ),
    );
  }

  renderInspectButton() {
    return dom.button(
      {
        onClick: e => this.inspect(),
        className: "debug-target-item__inspect-button",
      },
      "Inspect"
    );
  }

  render() {
    return dom.li(
      {
        className: "debug-target-item",
      },
      this.renderIcon(),
      this.renderInfo(),
      this.renderInspectButton(),
    );
  }
}

module.exports = DebugTargetItem;
