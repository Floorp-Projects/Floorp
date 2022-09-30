/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const DebugTargetItem = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/debugtarget/DebugTargetItem.js")
);

const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

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
          )
        );
  }
}

module.exports = DebugTargetList;
