/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const ExtensionDetail = createFactory(require("./debugtarget/ExtensionDetail"));
const TabDetail = createFactory(require("./debugtarget/TabDetail"));

const Actions = require("../actions/index");
const { DEBUG_TARGETS } = require("../constants");

/**
 * This component displays debug target.
 */
class DebugTargetItem extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      target: PropTypes.object.isRequired,
    };
  }

  inspect() {
    const { dispatch, target } = this.props;
    dispatch(Actions.inspectDebugTarget(target.type, target.id));
  }

  renderDetail() {
    const { target } = this.props;

    switch (target.type) {
      case DEBUG_TARGETS.EXTENSION:
        return ExtensionDetail({ target });
      case DEBUG_TARGETS.TAB:
        return TabDetail({ target });

      default:
        return null;
    }
  }

  renderIcon() {
    return dom.img({
      className: "debug-target-item__icon",
      src: this.props.target.icon,
    });
  }

  renderInfo() {
    const { target } = this.props;

    return dom.div(
      {
        className: "debug-target-item__info",
      },
      dom.div(
        {
          className: "debug-target-item__info__name ellipsis-text",
          title: target.name,
        },
        target.name
      ),
      this.renderDetail(),
    );
  }

  renderInspectButton() {
    return dom.button(
      {
        onClick: e => this.inspect(),
        className: "aboutdebugging-button",
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
