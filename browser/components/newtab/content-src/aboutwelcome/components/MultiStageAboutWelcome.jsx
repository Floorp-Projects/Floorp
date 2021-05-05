/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect, useRef } from "react";
import { Localized } from "./MSLocalized";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";
import { MultiStageScreen } from "./MultiStageScreen";
import { MultiStageProtonScreen } from "./MultiStageProtonScreen";
import {
  BASE_PARAMS,
  addUtmParams,
} from "../../asrouter/templates/FirstRun/addUtmParams";

// Amount of milliseconds for all transitions to complete (including delays).
const TRANSITION_OUT_TIME = 1000;

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

  // Allow "in" style to render to actually transition towards regular state,
  // which also makes using browser back/forward navigation skip transitions.
  const [transition, setTransition] = useState(props.transitions ? "in" : "");
  useEffect(() => {
    if (transition === "in") {
      requestAnimationFrame(() =>
        requestAnimationFrame(() => setTransition(""))
      );
    }
  }, [transition]);

  // Transition to next screen, opening about:home on last screen button CTA
  const handleTransition = () => {
    // Start transitioning things "out" immediately when moving forwards.
    setTransition(props.transitions ? "out" : "");

    // Actually move forwards after all transitions finish.
    setTimeout(
      () => {
        if (index < props.screens.length - 1) {
          setTransition(props.transitions ? "in" : "");
          setScreenIndex(prevState => prevState + 1);
        } else {
          AboutWelcomeUtils.handleUserAction({
            type: "OPEN_ABOUT_PAGE",
            data: { args: "home", where: "current" },
          });
        }
      },
      props.transitions ? TRANSITION_OUT_TIME : 0
    );
  };

  // Update top sites with default sites by region when region is available
  const [region, setRegion] = useState(null);
  useEffect(() => {
    (async () => {
      setRegion(await window.AWGetRegion());
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
  const importTelemetrySent = useRef(false);
  const [topSites, setTopSites] = useState([]);
  useEffect(() => {
    (async () => {
      let DEFAULT_SITES = await window.AWGetDefaultSites();
      const importable = JSON.parse(await window.AWGetImportableSites());
      const showImportable = useImportable && importable.length >= 5;
      if (!importTelemetrySent.current) {
        AboutWelcomeUtils.sendImpressionTelemetry(`${props.message_id}_SITES`, {
          display: showImportable ? "importable" : "static",
          importable: importable.length,
        });
        importTelemetrySent.current = true;
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
      <div
        className={`outer-wrapper onboardingContainer ${props.design} transition-${transition}`}
        style={{
          backgroundImage: `url(${props.background_url})`,
        }}
      >
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
              design={props.design}
            />
          ) : null;
        })}
      </div>
    </React.Fragment>
  );
};

export const SecondaryCTA = props => {
  let targetElement = props.position
    ? `secondary_button_${props.position}`
    : `secondary_button`;
  return (
    <div
      className={
        props.position ? `secondary-cta ${props.position}` : "secondary-cta"
      }
    >
      <Localized text={props.content[targetElement].text}>
        <span />
      </Localized>
      <Localized text={props.content[targetElement].label}>
        <button
          className="secondary text-link"
          value={targetElement}
          onClick={props.handleAction}
        />
      </Localized>
    </div>
  );
};

export const StepsIndicator = props => {
  let steps = [];
  for (let i = 0; i < props.totalNumberOfScreens; i++) {
    let className = i === props.order ? "current" : "";
    steps.push(<div key={i} className={`indicator ${className}`} />);
  }
  return steps;
};

export class WelcomeScreen extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleAction = this.handleAction.bind(this);
    this.state = { alternateContent: "" };
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

    // Wait until we become default browser to continue rest of action.
    if (action.waitForDefault) {
      // Update the UI to show additional "waiting" content.
      this.setState({ alternateContent: "waiting_for_default" });

      // Keep checking frequently as we want the UI to be responsive.
      await new Promise(resolve =>
        (async function checkDefault() {
          if (await window.AWIsDefaultBrowser()) {
            resolve();
          } else {
            setTimeout(checkDefault, 100);
          }
        })()
      );

      AboutWelcomeUtils.sendActionTelemetry(props.messageId, "default_browser");
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

  render() {
    // Use the provided content or switch to an alternate one.
    const { content, topSites } = this.props;
    let newContent = content;
    if (content[this.state.alternateContent]) {
      newContent = {
        ...content,
        ...content[this.state.alternateContent],
      };
    }

    if (this.props.design === "proton") {
      return (
        <MultiStageProtonScreen
          content={newContent}
          id={this.props.id}
          order={this.props.order}
          activeTheme={this.props.activeTheme}
          totalNumberOfScreens={this.props.totalNumberOfScreens - 1}
          handleAction={this.handleAction}
          design={this.props.design}
        />
      );
    }
    return (
      <MultiStageScreen
        content={newContent}
        id={this.props.id}
        order={this.props.order}
        topSites={topSites}
        activeTheme={this.props.activeTheme}
        totalNumberOfScreens={this.props.totalNumberOfScreens}
        handleAction={this.handleAction}
      />
    );
  }
}
