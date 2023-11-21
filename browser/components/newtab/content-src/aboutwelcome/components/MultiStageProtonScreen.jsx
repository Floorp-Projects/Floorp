/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useState } from "react";
import { Localized } from "./MSLocalized";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";
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
import { EmbeddedMigrationWizard } from "./EmbeddedMigrationWizard";
import { AddonsPicker } from "./AddonsPicker";
import { LinkParagraph } from "./LinkParagraph";

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
      forceHideStepsIndicator={props.forceHideStepsIndicator}
      ariaRole={props.ariaRole}
      aboveButtonStepsIndicator={props.aboveButtonStepsIndicator}
    />
  );
};

export const ProtonScreenActionButtons = props => {
  const { content, addonName, activeMultiSelect } = props;
  const defaultValue = content.checkbox?.defaultValue;

  const [isChecked, setIsChecked] = useState(defaultValue || false);
  const buttonRef = React.useRef(null);

  const shouldFocusButton = content?.primary_button?.should_focus_button;

  useEffect(() => {
    if (shouldFocusButton) {
      buttonRef.current?.focus();
    }
  }, [shouldFocusButton]);

  if (
    !content.primary_button &&
    !content.secondary_button &&
    !content.additional_button
  ) {
    return null;
  }

  // If we have a multi-select screen, we want to disable the primary button
  // until the user has selected at least one item.
  const isPrimaryDisabled = primaryDisabledValue =>
    primaryDisabledValue === "hasActiveMultiSelect"
      ? !(activeMultiSelect?.length > 0)
      : primaryDisabledValue;

  return (
    <div
      className={`action-buttons ${
        content.additional_button ? "additional-cta-container" : ""
      }`}
      flow={content.additional_button?.flow}
    >
      <Localized text={content.primary_button?.label}>
        <button
          ref={buttonRef}
          className={`${content.primary_button?.style ?? "primary"}${
            content.primary_button?.has_arrow_icon ? " arrow-icon" : ""
          }`}
          // Whether or not the checkbox is checked determines which action
          // should be handled. By setting value here, we indicate to
          // this.handleAction() where in the content tree it should take
          // the action to execute from.
          value={isChecked ? "checkbox" : "primary_button"}
          disabled={isPrimaryDisabled(content.primary_button?.disabled)}
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
    isVideoOnboarding,
    isAddonsPicker
  ) {
    const screenClass = `screen-${this.props.order % 2 !== 0 ? 1 : 2}`;

    if (isVideoOnboarding) {
      return "with-video";
    }

    if (isAddonsPicker) {
      return "addons-picker";
    }

    return `${isFirstScreen ? `dialog-initial` : ``} ${
      isLastScreen ? `dialog-last` : ``
    } ${includeNoodles ? `with-noodles` : ``} ${screenClass}`;
  }

  renderTitle({ title, title_logo }) {
    if (title_logo) {
      const { alignment, ...rest } = title_logo;
      return (
        <div
          className="inline-icon-container"
          alignment={alignment ?? "center"}
        >
          {this.renderPicture({ ...rest })}
          <Localized text={title}>
            <h1 id="mainContentHeader" />
          </Localized>
        </div>
      );
    }
    return (
      <Localized text={title}>
        <h1 id="mainContentHeader" />
      </Localized>
    );
  }

  renderPicture({
    imageURL = "chrome://branding/content/about-logo.svg",
    darkModeImageURL,
    reducedMotionImageURL,
    darkModeReducedMotionImageURL,
    alt = "",
    width,
    height,
    marginBlock,
    marginInline,
    className = "logo-container",
  }) {
    function getLoadingStrategy() {
      for (let url of [
        imageURL,
        darkModeImageURL,
        reducedMotionImageURL,
        darkModeReducedMotionImageURL,
      ]) {
        if (AboutWelcomeUtils.getLoadingStrategyFor(url) === "lazy") {
          return "lazy";
        }
      }
      return "eager";
    }

    return (
      <picture className={className} style={{ marginInline, marginBlock }}>
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
        <Localized text={alt}>
          <div className="sr-only logo-alt" />
        </Localized>
        <img
          className="brand-logo"
          style={{ height, width }}
          src={imageURL}
          alt=""
          loading={getLoadingStrategy()}
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
        content.tiles.type === "addons-picker" &&
        content.tiles.data ? (
          <AddonsPicker
            content={content}
            message_id={this.props.messageId}
            handleAction={this.props.handleAction}
          />
        ) : null}
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
        {content.tiles && content.tiles.type === "migration-wizard" ? (
          <EmbeddedMigrationWizard handleAction={this.props.handleAction} />
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
    const { size, marginBlock, marginInline, label } =
      this.props.content.dismiss_button;
    return (
      <button
        className="dismiss-button"
        onClick={this.props.handleAction}
        value="dismiss_button"
        data-l10n-id={label?.string_id || "spotlight-dialog-close-button"}
        button-size={size}
        style={{ marginBlock, marginInline }}
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
        data-l10n-id={
          content.steps_indicator?.string_id ||
          "onboarding-welcome-steps-indicator-label"
        }
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
        className={`section-secondary ${
          content.hide_secondary_section ? "with-secondary-section-hidden" : ""
        }`}
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
          </React.Fragment>
        )}
      </div>
    );
  }

  renderOrderedContent(content) {
    const elements = [];
    for (const item of content) {
      switch (item.type) {
        case "text":
          elements.push(
            <LinkParagraph
              text_content={item}
              handleAction={this.props.handleAction}
            />
          );
          break;
        case "image":
          elements.push(
            this.renderPicture({
              imageURL: item.url,
              darkModeImageURL: item.darkModeImageURL,
              height: item.height,
              width: item.width,
              alt: item.alt_text,
              marginInline: item.marginInline,
              className: "inline-image",
            })
          );
      }
    }
    return <>{elements}</>;
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
      forceHideStepsIndicator,
      ariaRole,
      aboveButtonStepsIndicator,
    } = this.props;
    const includeNoodles = content.has_noodles;
    // The default screen position is "center"
    const isCenterPosition = content.position === "center" || !content.position;
    const hideStepsIndicator =
      autoAdvance ||
      content?.video_container ||
      isSingleScreen ||
      forceHideStepsIndicator;
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
          content?.video_container,
          content.tiles?.type === "addons-picker"
        )
      : "";
    const isEmbeddedMigration = content.tiles?.type === "migration-wizard";

    return (
      <main
        className={`screen ${this.props.id || ""}
          ${screenClassName} ${textColorClass}`}
        role={ariaRole ?? "alertdialog"}
        layout={content.layout}
        pos={content.position || "center"}
        tabIndex="-1"
        aria-labelledby="mainContentHeader"
        ref={input => {
          this.mainContentHeader = input;
        }}
      >
        {isCenterPosition ? null : this.renderSecondarySection(content)}
        <div
          className={`section-main ${
            isEmbeddedMigration ? "embedded-migration" : ""
          }`}
          hide-secondary-section={
            content.hide_secondary_section
              ? String(content.hide_secondary_section)
              : null
          }
          role="document"
        >
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
            style={{
              background:
                content.background && isCenterPosition
                  ? content.background
                  : null,
              width:
                content.width && content.position !== "split"
                  ? content.width
                  : null,
            }}
          >
            {content.logo ? this.renderPicture(content.logo) : null}

            {isRtamo ? (
              <div className="rtamo-icon">
                <img
                  className={`${isTheme ? "rtamo-theme-icon" : "brand-logo"}`}
                  src={this.props.iconURL}
                  loading={AboutWelcomeUtils.getLoadingStrategyFor(
                    this.props.iconURL
                  )}
                  alt=""
                  role="presentation"
                />
              </div>
            ) : null}

            <div className="main-content-inner">
              <div className={`welcome-text ${content.title_style || ""}`}>
                {content.title ? this.renderTitle(content) : null}

                {content.subtitle ? (
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
                ) : null}
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
              {content.above_button_content
                ? this.renderOrderedContent(content.above_button_content)
                : null}
              {this.renderContentTiles()}
              {this.renderLanguageSwitcher()}
              {!hideStepsIndicator && aboveButtonStepsIndicator
                ? this.renderStepsIndicator()
                : null}
              <ProtonScreenActionButtons
                content={content}
                addonName={this.props.addonName}
                handleAction={this.props.handleAction}
                activeMultiSelect={this.props.activeMultiSelect}
              />
            </div>
            {!hideStepsIndicator && !aboveButtonStepsIndicator
              ? this.renderStepsIndicator()
              : null}
          </div>
          {content.dismiss_button ? this.renderDismissButton() : null}
        </div>
        <Localized text={content.info_text}>
          <span className="info-text" />
        </Localized>
      </main>
    );
  }
}
