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

  render() {
    const { autoClose, content, totalNumberOfScreens: total } = this.props;
    const windowObj = this.props.windowObj || window;
    const isWelcomeScreen = this.props.order === 0;
    const isLastScreen = this.props.order === total;
    const autoCloseTime = 20000;
    // Assign proton screen style 'screen-1' or 'screen-2' by checking
    // if screen order is even or odd.
    const screenClassName = isWelcomeScreen
      ? "screen-0"
      : `${this.props.order === 1 ? `dialog-initial` : ``} ${
          this.props.order === total ? `dialog-last` : ``
        } screen-${this.props.order % 2 !== 0 ? 1 : 2}`;

    if (isLastScreen && autoClose) {
      let currentURL = windowObj.location.href;
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

    return (
      <main
        className={`screen ${this.props.id} ${screenClassName}`}
        role="dialog"
        tabIndex="-1"
        aria-labelledby="mainContentHeader"
        ref={input => {
          this.mainContentHeader = input;
        }}
      >
        {isWelcomeScreen ? (
          <div className="section-left">
            <div className="message-text">
              <div className="spacer-top" />
              <Localized text={content.subtitle}>
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
        <div className="section-main">
          {content.secondary_button_top ? (
            <SecondaryCTA
              content={content}
              handleAction={this.props.handleAction}
              position="top"
            />
          ) : null}
          <div className={`noodle orange-L`} />
          <div className={`noodle purple-C`} />
          {isWelcomeScreen ? <div className={`noodle solid-L`} /> : null}
          <div className={`noodle outline-L`} />
          <div className={`noodle yellow-circle`} />
          <div
            className={`main-content ${
              isLastScreen && autoClose ? "no-steps" : ""
            }`}
          >
            <div className={`brand-logo ${content.hideLogo ? "hide" : ""}`} />
            {isLastScreen && content.hasFancyTitle ? (
              <div className="confetti" />
            ) : null}
            <div className="main-content-inner">
              <div
                className={`welcome-text ${
                  content.hasFancyTitle ? "fancy-headings" : ""
                }`}
              >
                <Localized text={content.title}>
                  <h1 id="mainContentHeader" />
                </Localized>
                {!isWelcomeScreen ? (
                  <Localized text={content.subtitle}>
                    <h2 />
                  </Localized>
                ) : null}
              </div>
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
            {!(isWelcomeScreen || (autoClose && isLastScreen)) ? (
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
            ) : null}
          </div>
        </div>
      </main>
    );
  }
}
