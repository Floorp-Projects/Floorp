/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("resource://devtools/client/shared/vendor/react.js");
const ReactDOM = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = React.createFactory(FluentReact.Localized);

/**
 * Helper functional component to render an accessible text progressbar.
 * @param {Object} props
 *        - id for the progressbar element
 *        - fluentId: localized string id
 */
function TextProgressBar({ id, fluentId }) {
  return Localized(
    {
      id: fluentId,
      attrs: { "aria-valuetext": true },
    },
    ReactDOM.span({
      id,
      key: id,
      role: "progressbar",
    })
  );
}

class AuditProgressOverlay extends React.Component {
  static get propTypes() {
    return {
      auditing: PropTypes.array.isRequired,
      total: PropTypes.number,
      percentage: PropTypes.number,
    };
  }

  render() {
    const { auditing, percentage, total } = this.props;
    if (auditing.length === 0) {
      return null;
    }

    const id = "audit-progress-container";

    if (total == null) {
      return TextProgressBar({
        id,
        fluentId: "accessibility-progress-initializing",
      });
    }

    if (percentage === 100) {
      return TextProgressBar({
        id,
        fluentId: "accessibility-progress-finishing",
      });
    }

    return ReactDOM.span(
      {
        id,
        key: id,
      },
      Localized({
        id: "accessibility-progress-progressbar",
        $nodeCount: total,
      }),
      ReactDOM.progress({
        max: 100,
        value: percentage,
        className: "audit-progress-progressbar",
        "aria-labelledby": id,
      })
    );
  }
}

const mapStateToProps = ({ audit: { auditing, progress } }) => {
  const { total, percentage } = progress || {};
  return { auditing, total, percentage };
};

module.exports = connect(mapStateToProps)(AuditProgressOverlay);
