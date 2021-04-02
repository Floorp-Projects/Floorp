/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";
import { Localized } from "./MSLocalized";
import { Zap } from "./Zap";
import { HelpText } from "./HelpText";
import { SecondaryCTA, StepsIndicator } from "./MultiStageAboutWelcome";

export class MultiStageScreen extends React.PureComponent {
  renderTiles() {
    switch (this.props.content.tiles.type) {
      case "topsites":
        return this.props.topSites && this.props.topSites.data ? (
          <div
            className={`tiles-container ${
              this.props.content.tiles.info ? "info" : ""
            }`}
          >
            <div
              className="tiles-topsites-section"
              name="topsites-section"
              id="topsites-section"
              aria-labelledby="helptext"
              role="region"
            >
              {this.props.topSites.data
                .slice(0, 5)
                .map(({ icon, label, title }) => (
                  <div
                    className="site"
                    key={icon + label}
                    aria-label={title ? title : label}
                    role="img"
                  >
                    <div
                      className="icon"
                      style={
                        icon
                          ? {
                              backgroundColor: "transparent",
                              backgroundImage: `url(${icon})`,
                            }
                          : {}
                      }
                    >
                      {icon ? "" : label && label[0].toUpperCase()}
                    </div>
                    {this.props.content.tiles.showTitles && (
                      <div className="host">{title || label}</div>
                    )}
                  </div>
                ))}
            </div>
          </div>
        ) : null;
      case "theme":
        return this.props.content.tiles.data ? (
          <div className="tiles-theme-container">
            <div>
              <fieldset className="tiles-theme-section">
                <Localized text={this.props.content.subtitle}>
                  <legend className="sr-only" />
                </Localized>
                {this.props.content.tiles.data.map(
                  ({ theme, label, tooltip, description }) => (
                    <Localized
                      key={theme + label}
                      text={typeof tooltip === "object" ? tooltip : {}}
                    >
                      <label
                        className={`theme${
                          theme === this.props.activeTheme ? " selected" : ""
                        }`}
                        title={theme + label}
                      >
                        <Localized
                          text={
                            typeof description === "object" ? description : {}
                          }
                        >
                          <input
                            type="radio"
                            value={theme}
                            name="theme"
                            checked={theme === this.props.activeTheme}
                            className="sr-only input"
                            onClick={this.props.handleAction}
                            data-l10n-attrs="aria-description"
                          />
                        </Localized>
                        <div className={`icon ${theme}`} />
                        {label && (
                          <Localized text={label}>
                            <div className="text" />
                          </Localized>
                        )}
                      </label>
                    </Localized>
                  )
                )}
              </fieldset>
            </div>
          </div>
        ) : null;
      case "video":
        return this.props.content.tiles.source ? (
          <div
            className={`tiles-media-section ${this.props.content.tiles.media_type}`}
          >
            <div className="fade" />
            <video
              className="media"
              autoPlay="true"
              loop="true"
              muted="true"
              src={
                AboutWelcomeUtils.hasDarkMode()
                  ? this.props.content.tiles.source.dark
                  : this.props.content.tiles.source.default
              }
            />
          </div>
        ) : null;
      case "image":
        return this.props.content.tiles.source ? (
          <div className={`${this.props.content.tiles.media_type}`}>
            <img
              src={
                AboutWelcomeUtils.hasDarkMode() &&
                this.props.content.tiles.source.dark
                  ? this.props.content.tiles.source.dark
                  : this.props.content.tiles.source.default
              }
              role="presentation"
              alt=""
            />
          </div>
        ) : null;
    }
    return null;
  }

  render() {
    const { content, topSites } = this.props;

    const showImportableSitesDisclaimer =
      content.tiles &&
      content.tiles.type === "topsites" &&
      topSites &&
      topSites.showImportable;

    return (
      <main className={`screen ${this.props.id}`}>
        {content.secondary_button_top ? (
          <SecondaryCTA
            content={content}
            handleAction={this.props.handleAction}
            position="top"
          />
        ) : null}
        <div
          className={`brand-logo ${
            content.secondary_button_top ? "cta-top" : ""
          }`}
        />
        <div className="welcome-text">
          <Zap hasZap={content.zap} text={content.title} />
          <Localized text={content.subtitle}>
            <h2 />
          </Localized>
        </div>
        {content.tiles ? this.renderTiles() : null}
        <div>
          <Localized
            text={content.primary_button ? content.primary_button.label : null}
          >
            <button
              className="primary"
              value="primary_button"
              onClick={this.props.handleAction}
            />
          </Localized>
        </div>
        {content.help_text && content.help_text.position === "default" ? (
          <HelpText
            text={content.help_text.text}
            position={content.help_text.position}
            hasImg={content.help_text.img}
          />
        ) : null}
        {content.secondary_button ? (
          <SecondaryCTA
            content={content}
            handleAction={this.props.handleAction}
          />
        ) : null}
        <nav
          className={
            (content.help_text && content.help_text.position === "footer") ||
            showImportableSitesDisclaimer
              ? "steps has-helptext"
              : "steps"
          }
          data-l10n-id={"onboarding-welcome-steps-indicator"}
          data-l10n-args={`{"current": ${parseInt(this.props.order, 10) +
            1}, "total": ${this.props.totalNumberOfScreens}}`}
        >
          {/* These empty elements are here to help trigger the nav for screen readers. */}
          <br />
          <p />
          <StepsIndicator
            order={this.props.order}
            totalNumberOfScreens={this.props.totalNumberOfScreens}
          />
        </nav>
        {(content.help_text && content.help_text.position === "footer") ||
        showImportableSitesDisclaimer ? (
          <HelpText
            text={content.help_text.text}
            position={content.help_text.position}
            hasImg={content.help_text.img}
          />
        ) : null}
      </main>
    );
  }
}
