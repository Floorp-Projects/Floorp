/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

export class SectionTitle extends React.PureComponent {
  render() {
    const {
      header: { title, subtitle },
    } = this.props;
    return (
      <div className="ds-section-title">
        <div className="title">{title}</div>
        {subtitle ? <div className="subtitle">{subtitle}</div> : null}
      </div>
    );
  }
}
