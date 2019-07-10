/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { connect } from "react-redux";
import { TopSites as OldTopSites } from "content-src/components/TopSites/TopSites";
import React from "react";

export class _TopSites extends React.PureComponent {
  render() {
    const header = this.props.header || {};
    return (
      <div className="ds-top-sites">
        <OldTopSites isFixed={true} title={header.title} />
      </div>
    );
  }
}

export const TopSites = connect(state => ({ TopSites: state.TopSites }))(
  _TopSites
);
