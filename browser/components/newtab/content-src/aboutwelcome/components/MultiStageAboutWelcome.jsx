/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useCallback, useEffect } from "react";
import { Localized } from "./MSLocalized";
import { Zap } from "./Zap";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";
import { addUtmParams } from "../../asrouter/templates/FirstRun/addUtmParams";

const DEFAULT_SITES = [
  "youtube-com",
  "facebook-com",
  "amazon",
  "reddit-com",
  "wikipedia-org",
  "twitter-com",
].map(site => ({
  icon: `resource://activity-stream/data/content/tippytop/images/${site}@2x.png`,
  title: site.split("-")[0],
}));

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

  const useImportable = props.message_id.includes("IMPORTABLE");
  const [topSites, setTopSites] = useState(DEFAULT_SITES);
  useEffect(() => {
    (async () => {
      const importable = JSON.parse(await window.AWGetImportableSites());
      const showImportable = useImportable && importable.length >= 5;
      AboutWelcomeUtils.sendImpressionTelemetry(`${props.message_id}_SITES`, {
        display: showImportable ? "importable" : "static",
        importable: importable.length,
      });
      setTopSites(showImportable ? importable : DEFAULT_SITES);
    })();
  }, [useImportable]);

  return (
    <React.Fragment>
      <div className={`outer-wrapper multistageContainer`}>
        {props.screens.map(screen => {
          return index === screen.order ? (
            <WelcomeScreen
              id={screen.id}
              totalNumberOfScreens={props.screens.length}
              order={screen.order}
              content={screen.content}
              navigate={handleTransition}
              topSites={topSites}
              messageId={`${props.message_id}_${screen.id}`}
              UTMTerm={props.utm_term}
              flowParams={flowParams}
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
    let url = new URL(data.args);
    addUtmParams(url, `aboutwelcome-${UTMTerm}-screen`);

    if (action.addFlowParams && flowParams) {
      url.searchParams.append("device_id", flowParams.deviceId);
      url.searchParams.append("flow_id", flowParams.flowId);
      url.searchParams.append("flow_begin_time", flowParams.flowBeginTime);
    }

    data = { ...data, args: url.toString() };
    AboutWelcomeUtils.handleUserAction({ type, data });
  }

  highlightTheme(theme) {
    const themes = document.querySelectorAll("button.theme");
    themes.forEach(function(element) {
      element.classList.remove("selected");
      if (element.value === theme) {
        element.classList.add("selected");
      }
    });
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

    if (action.type === "OPEN_URL") {
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
      this.highlightTheme(event.currentTarget.value);
      window.AWSelectTheme(
        action.theme === "<event>" ? event.currentTarget.value : action.theme
      );
    }

    if (action.navigate) {
      props.navigate();
    }
  }

  renderSecondaryCTA(className) {
    return (
      <div className={`secondary-cta ${className}`}>
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
        return this.props.topSites ? (
          <Localized
            text={
              typeof this.props.content.tiles.tooltip === "object"
                ? this.props.content.tiles.tooltip
                : {}
            }
          >
            <div
              className={`tiles-container ${
                this.props.content.tiles.tooltip ? "info" : ""
              }`}
              title={this.props.content.tiles.tooltip}
            >
              <div className="tiles-topsites-section">
                {this.props.topSites
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
                        {icon ? "" : label[0].toUpperCase()}
                      </div>
                      {label && <div className="host">{label}</div>}
                    </div>
                  ))}
              </div>
            </div>
          </Localized>
        ) : null;
      case "theme":
        return this.props.content.tiles.data ? (
          <div className="tiles-theme-section">
            {this.props.content.tiles.data.map(({ theme, label }) => (
              <button
                className="theme"
                key={theme + label}
                value={theme}
                onClick={this.handleAction}
              >
                <div className={`icon ${theme}`} />
                {label && (
                  <Localized text={label}>
                    <div className="text" />
                  </Localized>
                )}
              </button>
            ))}
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

  render() {
    const { content } = this.props;
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
          className="steps"
          data-l10n-id={"onboarding-welcome-steps-indicator"}
          data-l10n-args={`{"current": ${parseInt(this.props.order, 10) +
            1}, "total": ${this.props.totalNumberOfScreens}}`}
        >
          {this.renderStepsIndicator()}
        </nav>
      </main>
    );
  }
}
