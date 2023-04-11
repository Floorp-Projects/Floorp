/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useState } from "react";
import { Localized } from "./MSLocalized";
import { Colorways } from "./MRColorways";
import { MobileDownloads } from "./MobileDownloads";
import { MultiSelect } from "./MultiSelect";
import { Themes } from "./Themes";
import {
  SecondaryCTA,
  StepsIndicator,
  ProgressBar,
} from "./MultiStageAboutWelcome";
import { LanguageSwitcher } from "./LanguageSwitcher";
import { CTAParagraph } from "./CTAParagraph";
import { HeroImage } from "./HeroImage";
import { OnboardingVideo } from "./OnboardingVideo";
import { AdditionalCTA } from "./AdditionalCTA";

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
      activeMultiSelect={props.activeMultiSelect}
      setActiveMultiSelect={props.setActiveMultiSelect}
      totalNumberOfScreens={props.totalNumberOfScreens}
      handleAction={props.handleAction}
      isFirstScreen={props.isFirstScreen}
      isLastScreen={props.isLastScreen}
      isSingleScreen={props.isSingleScreen}
      previousOrder={props.previousOrder}
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

export const ProtonScreenActionButtons = props => {
  const { content, addonName } = props;
  const defaultValue = content.checkbox?.defaultValue;

  const [isChecked, setIsChecked] = useState(defaultValue || false);

  if (
    !content.primary_button &&
    !content.secondary_button &&
    !content.additional_button
  ) {
    return null;
  }

  return (
    <div
      className={`action-buttons ${
        content.additional_button ? "additional-cta-container" : ""
      }`}
      flow={content.additional_button?.flow}
    >
      <Localized text={content.primary_button?.label}>
        <button
          className="primary"
          // Whether or not the checkbox is checked determines which action
          // should be handled. By setting value here, we indicate to
          // this.handleAction() where in the content tree it should take
          // the action to execute from.
          value={isChecked ? "checkbox" : "primary_button"}
          disabled={content.primary_button?.disabled === true}
          onClick={props.handleAction}
          data-l10n-args={
            addonName
              ? JSON.stringify({
                  "addon-name": addonName,
                })
              : ""
          }
        />
      </Localized>
      {content.additional_button ? (
        <AdditionalCTA content={content} handleAction={props.handleAction} />
      ) : null}
      {content.checkbox ? (
        <div className="checkbox-container">
          <input
            type="checkbox"
            id="action-checkbox"
            checked={isChecked}
            onChange={() => {
              setIsChecked(!isChecked);
            }}
          ></input>
          <Localized text={content.checkbox.label}>
            <label htmlFor="action-checkbox"></label>
          </Localized>
        </div>
      ) : null}
      {content.secondary_button ? (
        <SecondaryCTA content={content} handleAction={props.handleAction} />
      ) : null}
    </div>
  );
};

export class ProtonScreen extends React.PureComponent {
  componentDidMount() {
    this.mainContentHeader.focus();
  }

  getScreenClassName(
    isFirstScreen,
    isLastScreen,
    includeNoodles,
    isVideoOnboarding
  ) {
    const screenClass = `screen-${this.props.order % 2 !== 0 ? 1 : 2}`;

    if (isVideoOnboarding) return "with-video";

    return `${isFirstScreen ? `dialog-initial` : ``} ${
      isLastScreen ? `dialog-last` : ``
    } ${includeNoodles ? `with-noodles` : ``} ${screenClass}`;
  }

  renderLogo({
    imageURL = "chrome://branding/content/about-logo.svg",
    darkModeImageURL,
    reducedMotionImageURL,
    darkModeReducedMotionImageURL,
    alt = "",
    height,
  }) {
    return (
      <picture className="logo-container">
        {darkModeReducedMotionImageURL ? (
          <source
            srcSet={darkModeReducedMotionImageURL}
            media="(prefers-color-scheme: dark) and (prefers-reduced-motion: reduce)"
          />
        ) : null}
        {darkModeImageURL ? (
          <source
            srcSet={darkModeImageURL}
            media="(prefers-color-scheme: dark)"
          />
        ) : null}
        {reducedMotionImageURL ? (
          <source
            srcSet={reducedMotionImageURL}
            media="(prefers-reduced-motion: reduce)"
          />
        ) : null}
        <img
          className="brand-logo"
          style={{ height }}
          src={imageURL}
          alt={alt}
          role={alt ? null : "presentation"}
        />
      </picture>
    );
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
        {content.tiles &&
        content.tiles.type === "multiselect" &&
        content.tiles.data ? (
          <MultiSelect
            content={content}
            activeMultiSelect={this.props.activeMultiSelect}
            setActiveMultiSelect={this.props.setActiveMultiSelect}
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

  renderStepsIndicator() {
    const currentStep = (this.props.order ?? 0) + 1;
    const previousStep = (this.props.previousOrder ?? -1) + 1;
    const { content, totalNumberOfScreens: total } = this.props;
    return (
      <div
        id="steps"
        className={`steps${content.progress_bar ? " progress-bar" : ""}`}
        data-l10n-id={"onboarding-welcome-steps-indicator-label"}
        data-l10n-args={JSON.stringify({
          current: currentStep,
          total: total ?? 0,
        })}
        data-l10n-attrs="aria-label"
        role="progressbar"
        aria-valuenow={currentStep}
        aria-valuemin={1}
        aria-valuemax={total}
      >
        {content.progress_bar ? (
          <ProgressBar
            step={currentStep}
            previousStep={previousStep}
            totalNumberOfScreens={total}
          />
        ) : (
          <StepsIndicator
            order={this.props.order}
            totalNumberOfScreens={total}
          />
        )}
      </div>
    );
  }

  renderSecondarySection(content) {
    return (
      <div
        className="section-secondary"
        style={
          content.background
            ? {
                background: content.background,
                "--mr-secondary-background-position-y":
                  content.split_narrow_bkg_position,
              }
            : {}
        }
      >
        <Localized text={content.image_alt_text}>
          <div className="sr-only image-alt" role="img" />
        </Localized>
        {content.hero_image ? (
          <HeroImage url={content.hero_image.url} />
        ) : (
          <React.Fragment>
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
          </React.Fragment>
        )}
      </div>
    );
  }

  render() {
    const {
      autoAdvance,
      content,
      isRtamo,
      isTheme,
      isFirstScreen,
      isLastScreen,
      isSingleScreen,
    } = this.props;
    const includeNoodles = content.has_noodles;
    // The default screen position is "center"
    const isCenterPosition = content.position === "center" || !content.position;
    const hideStepsIndicator =
      autoAdvance || content?.video_container || isSingleScreen;
    const textColorClass = content.text_color
      ? `${content.text_color}-text`
      : "";
    // Assign proton screen style 'screen-1' or 'screen-2' to centered screens
    // by checking if screen order is even or odd.
    const screenClassName = isCenterPosition
      ? this.getScreenClassName(
          isFirstScreen,
          isLastScreen,
          includeNoodles,
          content?.video_container
        )
      : "";

    return (
      <main
        className={`screen ${this.props.id || ""}
          ${screenClassName} ${textColorClass}`}
        role="alertdialog"
        pos={content.position || "center"}
        tabIndex="-1"
        aria-labelledby="mainContentHeader"
        ref={input => {
          this.mainContentHeader = input;
        }}
      >
        {isCenterPosition ? null : this.renderSecondarySection(content)}
        <div className="section-main" role="document">
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
            {content.logo ? this.renderLogo(content.logo) : null}

            {isRtamo ? (
              <div className="rtamo-icon">
                <img
                  className={`${isTheme ? "rtamo-theme-icon" : "brand-logo"}`}
                  src={this.props.iconURL}
                  role="presentation"
                  alt=""
                />
              </div>
            ) : null}

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
                    aria-flowto={
                      this.props.messageId?.includes("FEATURE_TOUR")
                        ? "steps"
                        : ""
                    }
                  />
                </Localized>
                {content.cta_paragraph ? (
                  <CTAParagraph
                    content={content.cta_paragraph}
                    handleAction={this.props.handleAction}
                  />
                ) : null}
              </div>
              {content.video_container ? (
                <OnboardingVideo
                  content={content.video_container}
                  handleAction={this.props.handleAction}
                />
              ) : null}
              {this.renderContentTiles()}
              {this.renderLanguageSwitcher()}
              <ProtonScreenActionButtons
                content={content}
                addonName={this.props.addonName}
                handleAction={this.props.handleAction}
              />
            </div>
            {!hideStepsIndicator ? this.renderStepsIndicator() : null}
          </div>
          {content.dismiss_button ? this.renderDismissButton() : null}
        </div>
      </main>
    );
  }
}
