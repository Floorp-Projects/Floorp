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

const Actions = require("resource://devtools/client/aboutdebugging/src/actions/index.js");
const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

/**
 * This component provides inspect button.
 */
class InspectAction extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      disabled: PropTypes.bool,
      target: Types.debugTarget.isRequired,
      title: PropTypes.string,
    };
  }

  inspect() {
    const { dispatch, target } = this.props;
    dispatch(Actions.inspectDebugTarget(target.type, target.id));
  }

  render() {
    const { disabled, title } = this.props;

    return Localized(
      {
        id: "about-debugging-debug-target-inspect-button",
      },
      dom.button(
        {
          onClick: e => this.inspect(),
          className: "default-button  qa-debug-target-inspect-button",
          disabled,
          title,
        },
        "Inspect"
      )
    );
  }
}

module.exports = InspectAction;
