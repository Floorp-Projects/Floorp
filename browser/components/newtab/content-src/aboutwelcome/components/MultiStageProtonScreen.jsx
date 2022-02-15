/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "./MSLocalized";
import { Colorways } from "./Colorways";
import { Themes } from "./Themes";
import { SecondaryCTA, StepsIndicator } from "./MultiStageAboutWelcome";

export class MultiStageProtonScreen extends React.PureComponent {
  componentDidMount() {
    this.mainContentHeader.focus();
  }

  getLogoStyle(content) {
    if (!content.hide_logo) {
      const useDefaultLogo = !content.logo;
      const logoUrl = useDefaultLogo
        ? "chrome://branding/content/about-logo.svg"
        : content.logo.imageURL;
      const logoSize = useDefaultLogo ? "80px" : content.logo.size;
      return {
        background: `url('${logoUrl}') top center / ${logoSize} no-repeat`,
        height: logoSize,
        padding: `${logoSize} 0 10px`,
      };
    }
    return {};
  }

  handleAutoClose(windowObj, currentURL) {
    const autoCloseTime = 20000;
    setTimeout(function() {
      // set the timer to close last screen and redirect to about:home after 20 seconds
      const screenEl = windowObj.document.querySelector(".screen");
      if (
        windowObj.location.href === currentURL &&
        screenEl.className.includes("dialog-last")
      ) {
        windowObj.location.href = "about:home";
      }
    }, autoCloseTime);
  }

  getScreenClassName(
    isCornerPosition,
    isFirstCenteredScreen,
    isLastCenteredScreen
  ) {
    const screenClass = isCornerPosition
      ? ""
      : `screen-${this.props.order % 2 !== 0 ? 1 : 2}`;
    return `${isFirstCenteredScreen ? `dialog-initial` : ``} ${
      isLastCenteredScreen ? `dialog-last` : ``
    } ${screenClass}`;
  }

  renderContentTiles() {
    const { content } = this.props;
    return (
      <React.Fragment>
        {content.tiles &&
        content.tiles.type === "colorway" &&
        content.tiles.colorways ? (
          <Colorways
            content={content}
            activeTheme={this.props.activeTheme}
            handleAction={this.props.handleAction}
          />
        ) : null}
        {content.tiles &&
        content.tiles.type === "theme" &&
        content.tiles.data ? (
          <Themes
            content={content}
            activeTheme={this.props.activeTheme}
            handleAction={this.props.handleAction}
          />
        ) : null}
      </React.Fragment>
    );
  }

  renderNoodles(includeNoodles, isCornerPosition) {
    return (
      <React.Fragment>
        {includeNoodles ? <div className={`noodle orange-L`} /> : null}
        {includeNoodles ? <div className={`noodle purple-C`} /> : null}
        {isCornerPosition ? <div className={`noodle solid-L`} /> : null}
        {includeNoodles ? <div className={`noodle outline-L`} /> : null}
        {includeNoodles ? <div className={`noodle yellow-circle`} /> : null}
      </React.Fragment>
    );
  }

  render() {
    const {
      autoClose,
      content,
      isRtamo,
      isTheme,
      isFirstCenteredScreen,
      isLastCenteredScreen,
      totalNumberOfScreens: total,
    } = this.props;
    const windowObj = this.props.windowObj || window;
    let currentURL = windowObj.location.href;
    const includeNoodles = content.has_noodles;
    const isCornerPosition = content.position === "corner";
    const hideStepsIndicator = autoClose || isCornerPosition;
    // Assign proton screen style 'screen-1' or 'screen-2' by checking
    // if screen order is even or odd.
    const screenClassName = this.getScreenClassName(
      isCornerPosition,
      isFirstCenteredScreen,
      isLastCenteredScreen
    );
    if (autoClose) {
      this.handleAutoClose(windowObj, currentURL);
    }

    return (
      <main
        className={`screen ${this.props.id || ""} ${screenClassName}`}
        role="dialog"
        pos={content.position || "center"}
        tabIndex="-1"
        aria-labelledby="mainContentHeader"
        ref={input => {
          this.mainContentHeader = input;
        }}
      >
        {isCornerPosition ? (
          <div className="section-left">
            <div className="message-text">
              <div className="spacer-top" />
              <Localized text={content.hero_text}>
                <h1 />
              </Localized>
              <div className="spacer-bottom" />
            </div>
            {content.help_text && content.help_text.text ? (
              <Localized text={content.help_text.text}>
                <span className="attrib-text" />
              </Localized>
            ) : null}
          </div>
        ) : null}
        <div className={`section-main ${includeNoodles ? "with-noodles" : ""}`}>
          {content.secondary_button_top ? (
            <SecondaryCTA
              content={content}
              handleAction={this.props.handleAction}
              position="top"
            />
          ) : null}
          {this.renderNoodles(includeNoodles, isCornerPosition)}
          <div
            className={`main-content ${hideStepsIndicator ? "no-steps" : ""}`}
          >
            <div
              className={`brand-logo ${content.hide_logo ? "hide" : ""}`}
              style={this.getLogoStyle(content)}
            />
            <div className={`${isRtamo ? "rtamo-icon" : "hide-rtamo-icon"}`}>
              <img
                className={`${isTheme ? "rtamo-theme-icon" : ""}`}
                src={this.props.iconURL}
                role="presentation"
                alt=""
              />
            </div>
            {content.has_fancy_title ? <div className="confetti" /> : null}
            <div className="main-content-inner">
              <div
                className={`welcome-text ${
                  content.has_fancy_title ? "fancy-headings" : ""
                }`}
              >
                <Localized text={content.title}>
                  <h1 id="mainContentHeader" />
                </Localized>
                {content.subtitle ? (
                  <Localized text={content.subtitle}>
                    <h2
                      data-l10n-args={JSON.stringify({
                        "addon-name": this.props.addonName,
                      })}
                    />
                  </Localized>
                ) : null}
              </div>
              {this.renderContentTiles()}
              <div>
                <Localized
                  text={
                    content.primary_button ? content.primary_button.label : null
                  }
                >
                  <button
                    className="primary"
                    value="primary_button"
                    onClick={this.props.handleAction}
                  />
                </Localized>
                {content.secondary_button ? (
                  <SecondaryCTA
                    content={content}
                    handleAction={this.props.handleAction}
                  />
                ) : null}
              </div>
            </div>
            {hideStepsIndicator ? null : (
              <nav
                className="steps"
                data-l10n-id={"onboarding-welcome-steps-indicator"}
                data-l10n-args={JSON.stringify({
                  current: this.props.order,
                  total,
                })}
              >
                {/* These empty elements are here to help trigger the nav for screen readers. */}
                <br />
                <p />
                <StepsIndicator
                  order={this.props.order - 1}
                  totalNumberOfScreens={total}
                />
              </nav>
            )}
          </div>
        </div>
      </main>
    );
  }
}
