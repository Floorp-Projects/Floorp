/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { connect } from "react-redux";
import React from "react";
import { SectionIntl } from "content-src/components/Sections/Sections";

export class _Highlights extends React.PureComponent {
  render() {
    const section = this.props.Sections.find(s => s.id === "highlights");
    if (!section || !section.enabled) {
      return null;
    }

    return (
      <div className="ds-highlights sections-list">
        <SectionIntl {...section} isFixed={true} />
      </div>
    );
  }
}

export const Highlights = connect(state => ({ Sections: state.Sections }))(
  _Highlights
);
