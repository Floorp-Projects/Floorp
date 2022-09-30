/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const InspectAction = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/debugtarget/InspectAction.js")
);

const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

/**
 * This component displays the inspect button for tabs.
 */
class TabAction extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      target: Types.debugTarget.isRequired,
    };
  }

  render() {
    const isZombieTab = this.props.target.details.isZombieTab;
    return Localized(
      {
        id: "about-debugging-zombie-tab-inspect-action-disabled",
        attrs: {
          // Show an explanatory title only if the action is disabled.
          title: isZombieTab,
        },
      },
      InspectAction({
        disabled: isZombieTab,
        dispatch: this.props.dispatch,
        target: this.props.target,
      })
    );
  }
}

module.exports = FluentReact.withLocalization(TabAction);
