/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../../types/index");

/**
 * This component displays debug target.
 */
class DebugTargetItem extends PureComponent {
  static get propTypes() {
    return {
      actionComponent: PropTypes.any.isRequired,
      additionalActionsComponent: PropTypes.any,
      detailComponent: PropTypes.any.isRequired,
      dispatch: PropTypes.func.isRequired,
      target: Types.debugTarget.isRequired,
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

  renderAdditionalActions() {
    const { additionalActionsComponent, dispatch, target } = this.props;

    if (!additionalActionsComponent) {
      return null;
    }

    return dom.section(
      {
        className: "debug-target-item__additional_actions",
      },
      additionalActionsComponent({ dispatch, target }),
    );
  }

  renderDetail() {
    const { detailComponent, target } = this.props;
    return detailComponent({ target });
  }

  renderIcon() {
    return dom.img({
      className: "debug-target-item__icon qa-debug-target-item-icon",
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
        className: "card debug-target-item qa-debug-target-item",
      },
      this.renderIcon(),
      this.renderName(),
      this.renderAction(),
      this.renderDetail(),
      this.renderAdditionalActions(),
    );
  }
}

module.exports = DebugTargetItem;
