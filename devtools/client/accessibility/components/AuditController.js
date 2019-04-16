/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class AuditController extends React.Component {
  static get propTypes() {
    return {
      accessible: PropTypes.object.isRequired,
      children: PropTypes.any,
    };
  }

  constructor(props) {
    super(props);

    const { accessible: { checks } } = props;
    this.state = {
      checks,
    };

    this.onAudited = this.onAudited.bind(this);
  }

  componentWillMount() {
    const { accessible } = this.props;
    accessible.on("audited", this.onAudited);
  }

  componentDidMount() {
    this.maybeRequestAudit();
  }

  componentDidUpdate() {
    this.maybeRequestAudit();
  }

  componentWillUnmount() {
    const { accessible } = this.props;
    accessible.off("audited", this.onAudited);
  }

  onAudited() {
    const { accessible } = this.props;
    if (!accessible.actorID) {
      // Accessible is being removed, stop listening for 'audited' events.
      accessible.off("audited", this.onAudited);
      return;
    }

    this.setState({ checks: accessible.checks });
  }

  maybeRequestAudit() {
    const { accessible } = this.props;
    if (!accessible.actorID) {
      // Accessible is being removed, stop listening for 'audited' events.
      accessible.off("audited", this.onAudited);
      return;
    }

    if (accessible.checks) {
      return;
    }

    accessible.audit().catch(error => {
      // Accessible actor was destroyed, connection closed.
      if (accessible.actorID) {
        console.warn(error);
      }
    });
  }

  render() {
    const { children } = this.props;
    const { checks } = this.state;

    return React.Children.only(React.cloneElement(children, { checks }));
  }
}

module.exports = AuditController;
