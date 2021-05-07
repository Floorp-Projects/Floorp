/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ThemesSection } from "content-src/components/CustomizeMenu/ThemesSection/ThemesSection";
import { BackgroundsSection } from "content-src/components/CustomizeMenu/BackgroundsSection/BackgroundsSection";
import { ContentSection } from "content-src/components/CustomizeMenu/ContentSection/ContentSection";
import { connect } from "react-redux";
import React from "react";
import { CSSTransition } from "react-transition-group";

export class _CustomizeMenu extends React.PureComponent {
  render() {
    return (
      <span>
        <button
          className="personalize-button"
          onClick={() => this.props.onOpen()}
          data-l10n-id="newtab-personalize-button-label"
        />
        <CSSTransition
          timeout={0}
          classNames="customize-animate"
          in={this.props.showing}
          appear={true}
        >
          <div
            className="customize-menu"
            role="dialog"
            data-l10n-id="newtab-personalize-dialog-label"
          >
            <button
              onClick={() => this.props.onClose()}
              className="close-button"
              data-l10n-id="newtab-custom-close-button"
            />
            <ThemesSection />
            <BackgroundsSection />
            <ContentSection
              openPreferences={this.props.openPreferences}
              setPref={this.props.setPref}
              enabledSections={this.props.enabledSections}
              pocketRegion={this.props.pocketRegion}
              mayHaveSponsoredTopSites={this.props.mayHaveSponsoredTopSites}
              mayHaveSponsoredStories={
                this.props.DiscoveryStream.config.show_spocs
              }
              dispatch={this.props.dispatch}
            />
          </div>
        </CSSTransition>
      </span>
    );
  }
}

export const CustomizeMenu = connect(state => ({
  DiscoveryStream: state.DiscoveryStream,
}))(_CustomizeMenu);
