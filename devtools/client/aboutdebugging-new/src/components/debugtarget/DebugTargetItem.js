/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * This component displays debug target.
 */
class DebugTargetItem extends PureComponent {
  static get propTypes() {
    return {
      actionComponent: PropTypes.any.isRequired,
      detailComponent: PropTypes.any.isRequired,
      dispatch: PropTypes.func.isRequired,
      target: PropTypes.object.isRequired,
    };
  }

  renderAction() {
    const { actionComponent, dispatch, target } = this.props;
    return dom.div(
      {
        className: "debug-target-item__action",
      },
      actionComponent({ dispatch, target }),
    );
  }

  renderDetail() {
    const { detailComponent, target } = this.props;
    return dom.div(
      {
        className: "debug-target-item__detail",
      },
      detailComponent({ target }),
    );
  }

  renderIcon() {
    return dom.img({
      className: "debug-target-item__icon",
      src: this.props.target.icon,
    });
  }

  renderName() {
    return dom.span(
      {
        className: "debug-target-item__name ellipsis-text",
        title: this.props.target.name,
      },
      this.props.target.name,
    );
  }

  render() {
    return dom.li(
      {
        className: "debug-target-item js-debug-target-item",
      },
      this.renderIcon(),
      this.renderName(),
      this.renderAction(),
      this.renderDetail(),
    );
  }
}

module.exports = DebugTargetItem;
