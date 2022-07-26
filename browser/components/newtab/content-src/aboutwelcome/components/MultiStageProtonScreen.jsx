/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect } from "react";
import { Localized } from "./MSLocalized";
import { Colorways } from "./Colorways";
import { MobileDownloads } from "./MobileDownloads";
import { Themes } from "./Themes";
import { SecondaryCTA, StepsIndicator } from "./MultiStageAboutWelcome";
import { LanguageSwitcher } from "./LanguageSwitcher";

export const MultiStageProtonScreen = props => {
  const { autoAdvance, handleAction, order } = props;
  useEffect(() => {
    if (autoAdvance) {
      const timer = setTimeout(() => {
        handleAction({
          currentTarget: {
            value: autoAdvance,
          },
          name: "AUTO_ADVANCE",
        });
      }, 20000);
      return () => clearTimeout(timer);
    }
    return () => {};
  }, [autoAdvance, handleAction, order]);

  return (
    <ProtonScreen
      content={props.content}
      id={props.id}
      order={props.order}
      activeTheme={props.activeTheme}
      totalNumberOfScreens={props.totalNumberOfScreens}
      handleAction={props.handleAction}
      isFirstCenteredScreen={props.isFirstCenteredScreen}
      isLastCenteredScreen={props.isLastCenteredScreen}
      stepOrder={props.stepOrder}
      autoAdvance={props.autoAdvance}
      isRtamo={props.isRtamo}
      addonName={props.addonName}
      isTheme={props.isTheme}
      iconURL={props.iconURL}
      messageId={props.messageId}
      negotiatedLanguage={props.negotiatedLanguage}
      langPackInstallPhase={props.langPackInstallPhase}
    />
  );
};

export class ProtonScreen extends React.PureComponent {
  componentDidMount() {
    this.mainContentHeader.focus();
  }

  getLogoStyle({ imageURL, height }) {
    let style = { height };
    style.backgroundImage = imageURL ? `url(${imageURL})` : null;
    return style;
  }

  getScreenClassName(
    isFirstCenteredScreen,
    isLastCenteredScreen,
    includeNoodles
  ) {
    const screenClass = `screen-${this.props.order % 2 !== 0 ? 1 : 2}`;
    return `${isFirstCenteredScreen ? `dialog-initial` : ``} ${
      isLastCenteredScreen ? `dialog-last` : ``
    } ${includeNoodles ? `with-noodles` : ``} ${screenClass}`;
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
        {content.tiles &&
        content.tiles.type === "mobile_downloads" &&
        content.tiles.data ? (
          <MobileDownloads
            data={content.tiles.data}
            handleAction={this.props.handleAction}
          />
        ) : null}
      </React.Fragment>
    );
  }

  renderNoodles() {
    return (
      <React.Fragment>
        <div className={"noodle orange-L"} />
        <div className={"noodle purple-C"} />
        <div className={"noodle solid-L"} />
        <div className={"noodle outline-L"} />
        <div className={"noodle yellow-circle"} />
      </React.Fragment>
    );
  }

  renderLanguageSwitcher() {
    return this.props.content.languageSwitcher ? (
      <LanguageSwitcher
        content={this.props.content}
        handleAction={this.props.handleAction}
        negotiatedLanguage={this.props.negotiatedLanguage}
        langPackInstallPhase={this.props.langPackInstallPhase}
        messageId={this.props.messageId}
      />
    ) : null;
  }

  renderDismissButton() {
    return (
      <button
        className="dismiss-button"
        onClick={this.props.handleAction}
        value="dismiss_button"
        data-l10n-id={"spotlight-dialog-close-button"}
      ></button>
    );
  }

  renderSecondarySection(content) {
    return (
      <div
        className="section-secondary"
        style={content.background ? { background: content.background } : {}}
      >
        <div className="message-text">
          <div className="spacer-top" />
          <Localized text={content.hero_text}>
            <h1 />
          </Localized>
          <div className="spacer-bottom" />
        </div>
        <Localized text={content.help_text}>
          <span className="attrib-text" />
        </Localized>
      </div>
    );
  }

  render() {
    const {
      autoAdvance,
      content,
      isRtamo,
      isTheme,
      isFirstCenteredScreen,
      isLastCenteredScreen,
      totalNumberOfScreens: total,
    } = this.props;
    const includeNoodles = content.has_noodles;
    // The default screen position is "center"
    const isCenterPosition = content.position === "center" || !content.position;
    const hideStepsIndicator =
      autoAdvance || (isFirstCenteredScreen && isLastCenteredScreen);
    const textColorClass = content.text_color
      ? `${content.text_color}-text`
      : "";
    // Assign proton screen style 'screen-1' or 'screen-2' to centered screens
    // by checking if screen order is even or odd.
    const screenClassName = isCenterPosition
      ? this.getScreenClassName(
          isFirstCenteredScreen,
          isLastCenteredScreen,
          includeNoodles
        )
      : "";

    return (
      <main
        className={`screen ${this.props.id ||
          ""} ${screenClassName} ${textColorClass}`}
        role="dialog"
        pos={content.position || "center"}
        tabIndex="-1"
        aria-labelledby="mainContentHeader"
        ref={input => {
          this.mainContentHeader = input;
        }}
      >
        {isCenterPosition ? null : this.renderSecondarySection(content)}
        <div className="section-main">
          {content.secondary_button_top ? (
            <SecondaryCTA
              content={content}
              handleAction={this.props.handleAction}
              position="top"
            />
          ) : null}
          {includeNoodles ? this.renderNoodles() : null}
          <div
            className={`main-content ${hideStepsIndicator ? "no-steps" : ""}`}
            style={
              content.background && isCenterPosition
                ? { background: content.background }
                : {}
            }
          >
            {content.dismiss_button ? this.renderDismissButton() : null}
            {content.logo ? (
              <div
                className={`brand-logo`}
                style={this.getLogoStyle(content.logo)}
              />
            ) : null}
            <div className={`${isRtamo ? "rtamo-icon" : "hide-rtamo-icon"}`}>
              <img
                className={`${isTheme ? "rtamo-theme-icon" : ""}`}
                src={this.props.iconURL}
                role="presentation"
                alt=""
              />
            </div>
            <div className="main-content-inner">
              <div className={`welcome-text ${content.title_style || ""}`}>
                <Localized text={content.title}>
                  <h1 id="mainContentHeader" />
                </Localized>
                <Localized text={content.subtitle}>
                  <h2
                    data-l10n-args={JSON.stringify({
                      "addon-name": this.props.addonName,
                      ...this.props.appAndSystemLocaleInfo?.displayNames,
                    })}
                  />
                </Localized>
              </div>
              {this.renderContentTiles()}
              {this.renderLanguageSwitcher()}
              <div className="action-buttons">
                <Localized text={content.primary_button?.label}>
                  <button
                    className="primary"
                    value="primary_button"
                    disabled={content.primary_button?.disabled === true}
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
                className={`steps ${
                  content.progress_bar ? "progress-bar" : ""
                }`}
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
                  order={this.props.stepOrder}
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
