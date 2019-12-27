/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { MESSAGE_LEVEL } = require("../../constants");

/**
 * This component is designed to wrap a warning / error log message
 * in the details tag to hide long texts and make the message expendable
 * out of the box.
 */
class DetailsLog extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node.isRequired,
      type: PropTypes.string,
    };
  }
  getLocalizationString() {
    const { type } = this.props;

    switch (type) {
      case MESSAGE_LEVEL.WARNING:
        return "about-debugging-message-details-label-warning";
      case MESSAGE_LEVEL.ERROR:
        return "about-debugging-message-details-label-error";
      default:
        return "about-debugging-message-details-label";
    }
  }

  render() {
    const { children } = this.props;

    return dom.details(
      {
        className: "details--log",
      },
      Localized(
        {
          id: this.getLocalizationString(),
        },
        dom.summary({}, this.getLocalizationString())
      ),
      children
    );
  }
}

module.exports = DetailsLog;
