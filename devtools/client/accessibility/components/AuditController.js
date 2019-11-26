/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class AuditController extends React.Component {
  static get propTypes() {
    return {
      accessibleFront: PropTypes.object.isRequired,
      children: PropTypes.any,
    };
  }

  constructor(props) {
    super(props);

    const {
      accessibleFront: { checks },
    } = props;
    this.state = {
      checks,
    };

    this.onAudited = this.onAudited.bind(this);
  }

  componentWillMount() {
    const { accessibleFront } = this.props;
    accessibleFront.on("audited", this.onAudited);
  }

  componentDidMount() {
    this.maybeRequestAudit();
  }

  componentDidUpdate() {
    this.maybeRequestAudit();
  }

  componentWillUnmount() {
    const { accessibleFront } = this.props;
    accessibleFront.off("audited", this.onAudited);
  }

  onAudited() {
    const { accessibleFront } = this.props;
    if (!accessibleFront.actorID) {
      // Accessible front is being removed, stop listening for 'audited' events.
      accessibleFront.off("audited", this.onAudited);
      return;
    }

    this.setState({ checks: accessibleFront.checks });
  }

  maybeRequestAudit() {
    const { accessibleFront } = this.props;
    if (!accessibleFront.actorID) {
      // Accessible front is being removed, stop listening for 'audited' events.
      accessibleFront.off("audited", this.onAudited);
      return;
    }

    if (accessibleFront.checks) {
      return;
    }

    accessibleFront.audit().catch(error => {
      // Accessible actor was destroyed, connection closed.
      if (accessibleFront.actorID) {
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
