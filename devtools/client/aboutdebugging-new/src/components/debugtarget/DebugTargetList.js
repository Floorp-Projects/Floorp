/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } =
  require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const DebugTargetItem = createFactory(require("./DebugTargetItem"));

const Types = require("../../types/index");

/**
 * This component displays list of debug target.
 */
class DebugTargetList extends PureComponent {
  static get propTypes() {
    return {
      actionComponent: PropTypes.any.isRequired,
      additionalActionsComponent: PropTypes.any,
      detailComponent: PropTypes.any.isRequired,
      dispatch: PropTypes.func.isRequired,
      targets: PropTypes.arrayOf(Types.debugTarget).isRequired,
    };
  }

  renderEmptyList() {
    return Localized(
      {
        id: "about-debugging-debug-target-list-empty",
      },
      dom.p(
        {
          className: "qa-debug-target-list-empty",
        },
        "Nothing yet."
      )
    );
  }

  render() {
    const {
      actionComponent,
      additionalActionsComponent,
      detailComponent,
      dispatch,
      targets,
    } = this.props;

    return targets.length === 0
      ? this.renderEmptyList()
      : dom.ul(
        {
          className: "debug-target-list qa-debug-target-list",
        },
        targets.map((target, key) =>
          DebugTargetItem({
            actionComponent,
            additionalActionsComponent,
            detailComponent,
            dispatch,
            key,
            target,
          })
        ),
      );
  }
}

module.exports = DebugTargetList;
