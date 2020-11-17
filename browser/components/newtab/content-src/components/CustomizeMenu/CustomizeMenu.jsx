/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ThemesSection } from "content-src/components/CustomizeMenu/ThemesSection/ThemesSection";
import { BackgroundsSection } from "content-src/components/CustomizeMenu/BackgroundsSection/BackgroundsSection";
import { ContentSection } from "content-src/components/CustomizeMenu/ContentSection/ContentSection";
import { connect } from "react-redux";
import React from "react";

export class _CustomizeMenu extends React.PureComponent {
  render() {
    return (
      <div className="customize-menu">
        <button
          onClick={this.props.onClose}
          className="button done"
          id="closeButton"
        >
          Done
        </button>
        <ThemesSection />
        <BackgroundsSection />
        <ContentSection />
      </div>
    );
  }
}

export const CustomizeMenu = connect(state => state.CustomizeMenu)(
  _CustomizeMenu
);
