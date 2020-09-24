/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useCallback, useEffect } from "react";
import { Localized } from "./MSLocalized";
import { Zap } from "./Zap";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";
import {
  BASE_PARAMS,
  addUtmParams,
} from "../../asrouter/templates/FirstRun/addUtmParams";

export const MultiStageAboutWelcome = props => {
  const [index, setScreenIndex] = useState(0);
  useEffect(() => {
    // Send impression ping when respective screen first renders
    props.screens.forEach(screen => {
      if (index === screen.order) {
        AboutWelcomeUtils.sendImpressionTelemetry(
          `${props.message_id}_${screen.id}`
        );
      }
    });

    // Remember that a new screen has loaded for browser navigation
    if (index > window.history.state) {
      window.history.pushState(index, "");
    }
  }, [index]);

  useEffect(() => {
    // Switch to the screen tracked in state (null for initial state)
    const handler = ({ state }) => setScreenIndex(Number(state));

    // Handle page load, e.g., going back to about:welcome from about:home
    handler(window.history);

    // Watch for browser back/forward button navigation events
    window.addEventListener("popstate", handler);
    return () => window.removeEventListener("popstate", handler);
  }, []);

  const [flowParams, setFlowParams] = useState(null);
  const { metricsFlowUri } = props;
  useEffect(() => {
    (async () => {
      if (metricsFlowUri) {
        setFlowParams(await AboutWelcomeUtils.fetchFlowParams(metricsFlowUri));
      }
    })();
  }, [metricsFlowUri]);

  // Transition to next screen, opening about:home on last screen button CTA
  const handleTransition =
    index < props.screens.length
      ? useCallback(() => setScreenIndex(prevState => prevState + 1), [])
      : AboutWelcomeUtils.handleUserAction({
          type: "OPEN_ABOUT_PAGE",
          data: { args: "home", where: "current" },
        });

  // Update top sites with default sites by region when region is available
  const [region, setRegion] = useState(null);
  useEffect(() => {
    (async () => {
      setRegion(await window.AWWaitForRegionChange());
    })();
  }, []);

  // Get the active theme so the rendering code can make it selected
  // by default.
  const [activeTheme, setActiveTheme] = useState(null);
  const [initialTheme, setInitialTheme] = useState(null);
  useEffect(() => {
    (async () => {
      let theme = await window.AWGetSelectedTheme();
      setInitialTheme(theme);
      setActiveTheme(theme);
    })();
  }, []);

  const useImportable = props.message_id.includes("IMPORTABLE");
  // Track whether we have already sent the importable sites impression telemetry
  const [importTelemetrySent, setImportTelemetrySent] = useState(null);
  const [topSites, setTopSites] = useState([]);
  useEffect(() => {
    (async () => {
      let DEFAULT_SITES = await window.AWGetDefaultSites();
      const importable = JSON.parse(await window.AWGetImportableSites());
      const showImportable = useImportable && importable.length >= 5;
      if (!importTelemetrySent) {
        AboutWelcomeUtils.sendImpressionTelemetry(`${props.message_id}_SITES`, {
          display: showImportable ? "importable" : "static",
          importable: importable.length,
        });
        setImportTelemetrySent(true);
      }
      setTopSites(
        showImportable
          ? { data: importable, showImportable }
          : { data: DEFAULT_SITES, showImportable }
      );
    })();
  }, [useImportable, region]);

  return (
    <React.Fragment>
      <div className={`outer-wrapper multistageContainer`}>
        {props.screens.map(screen => {
          return index === screen.order ? (
            <WelcomeScreen
              key={screen.id}
              id={screen.id}
              totalNumberOfScreens={props.screens.length}
              order={screen.order}
              content={screen.content}
              navigate={handleTransition}
              topSites={topSites}
              messageId={`${props.message_id}_${screen.id}`}
              UTMTerm={props.utm_term}
              flowParams={flowParams}
              activeTheme={activeTheme}
              initialTheme={initialTheme}
              setActiveTheme={setActiveTheme}
            />
          ) : null;
        })}
      </div>
    </React.Fragment>
  );
};

export class WelcomeScreen extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleAction = this.handleAction.bind(this);
  }

  handleOpenURL(action, flowParams, UTMTerm) {
    let { type, data } = action;
    if (type === "SHOW_FIREFOX_ACCOUNTS") {
      let params = {
        ...BASE_PARAMS,
        utm_term: `aboutwelcome-${UTMTerm}-screen`,
      };
      if (action.addFlowParams && flowParams) {
        params = {
          ...params,
          ...flowParams,
        };
      }
      data = { ...data, extraParams: params };
    } else if (type === "OPEN_URL") {
      let url = new URL(data.args);
      addUtmParams(url, `aboutwelcome-${UTMTerm}-screen`);
      if (action.addFlowParams && flowParams) {
        url.searchParams.append("device_id", flowParams.deviceId);
        url.searchParams.append("flow_id", flowParams.flowId);
        url.searchParams.append("flow_begin_time", flowParams.flowBeginTime);
      }
      data = { ...data, args: url.toString() };
    }
    AboutWelcomeUtils.handleUserAction({ type, data });
  }

  async handleAction(event) {
    let { props } = this;

    let targetContent =
      props.content[event.currentTarget.value] || props.content.tiles;
    if (!(targetContent && targetContent.action)) {
      return;
    }

    // Send telemetry before waiting on actions
    AboutWelcomeUtils.sendActionTelemetry(
      props.messageId,
      event.currentTarget.value
    );

    let { action } = targetContent;

    if (["OPEN_URL", "SHOW_FIREFOX_ACCOUNTS"].includes(action.type)) {
      this.handleOpenURL(action, props.flowParams, props.UTMTerm);
    } else if (action.type) {
      AboutWelcomeUtils.handleUserAction(action);
      // Wait until migration closes to complete the action
      if (action.type === "SHOW_MIGRATION_WIZARD") {
        await window.AWWaitForMigrationClose();
        AboutWelcomeUtils.sendActionTelemetry(props.messageId, "migrate_close");
      }
    }

    // A special tiles.action.theme value indicates we should use the event's value vs provided value.
    if (action.theme) {
      let themeToUse =
        action.theme === "<event>"
          ? event.currentTarget.value
          : this.props.initialTheme || action.theme;

      this.props.setActiveTheme(themeToUse);
      window.AWSelectTheme(themeToUse);
    }

    if (action.navigate) {
      props.navigate();
    }
  }

  renderSecondaryCTA(className) {
    return (
      <div
        className={className ? `secondary-cta ${className}` : `secondary-cta`}
      >
        <Localized text={this.props.content.secondary_button.text}>
          <span />
        </Localized>
        <Localized text={this.props.content.secondary_button.label}>
          <button
            className="secondary"
            value="secondary_button"
            onClick={this.handleAction}
          />
        </Localized>
      </div>
    );
  }

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
              aria-labelledby="topsites-disclaimer"
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
                            onClick={this.handleAction}
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
    }
    return null;
  }

  renderStepsIndicator() {
    let steps = [];
    for (let i = 0; i < this.props.totalNumberOfScreens; i++) {
      let className = i === this.props.order ? "current" : "";
      steps.push(<div key={i} className={`indicator ${className}`} />);
    }
    return steps;
  }

  renderDisclaimer() {
    if (
      this.props.content.tiles &&
      this.props.content.tiles.type === "topsites" &&
      this.props.topSites &&
      this.props.topSites.showImportable
    ) {
      return (
        <Localized text={this.props.content.disclaimer}>
          <p id="topsites-disclaimer" className="tiles-topsites-disclaimer" />
        </Localized>
      );
    }
    return null;
  }

  render() {
    const { content, topSites } = this.props;
    const hasSecondaryTopCTA =
      content.secondary_button && content.secondary_button.position === "top";
    return (
      <main className={`screen ${this.props.id}`}>
        {hasSecondaryTopCTA ? this.renderSecondaryCTA("top") : null}
        <div className={`brand-logo ${hasSecondaryTopCTA ? "cta-top" : ""}`} />
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
              onClick={this.handleAction}
            />
          </Localized>
        </div>
        {content.secondary_button && content.secondary_button.position !== "top"
          ? this.renderSecondaryCTA()
          : null}
        <nav
          className={
            content.tiles &&
            content.tiles.type === "topsites" &&
            topSites &&
            topSites.showImportable
              ? "steps has-disclaimer"
              : "steps"
          }
          data-l10n-id={"onboarding-welcome-steps-indicator"}
          data-l10n-args={`{"current": ${parseInt(this.props.order, 10) +
            1}, "total": ${this.props.totalNumberOfScreens}}`}
        >
          {/* These empty elements are here to help trigger the nav for screen readers. */}
          <br />
          <p />
          {this.renderStepsIndicator()}
        </nav>
        {this.renderDisclaimer()}
      </main>
    );
  }
}
