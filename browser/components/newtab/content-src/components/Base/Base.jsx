/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { ASRouterAdmin } from "content-src/components/ASRouterAdmin/ASRouterAdmin";
import { ASRouterUISurface } from "../../asrouter/asrouter-content";
import { ConfirmDialog } from "content-src/components/ConfirmDialog/ConfirmDialog";
import { connect } from "react-redux";
import { DiscoveryStreamBase } from "content-src/components/DiscoveryStreamBase/DiscoveryStreamBase";
import { ErrorBoundary } from "content-src/components/ErrorBoundary/ErrorBoundary";
import { CustomizeMenu } from "content-src/components/CustomizeMenu/CustomizeMenu";
import React from "react";
import { Search } from "content-src/components/Search/Search";
import { Sections } from "content-src/components/Sections/Sections";
import { CSSTransition } from "react-transition-group";

export const PrefsButton = ({ onClick, icon }) => (
  <div className="prefs-button">
    <button
      className={`icon ${icon || "icon-settings"}`}
      onClick={onClick}
      data-l10n-id="newtab-settings-button"
    />
  </div>
);

export const PersonalizeButton = ({ onClick }) => (
  <button
    className="personalize-button"
    onClick={onClick}
    data-l10n-id="newtab-personalize-button-label"
  />
);

// Returns a function will not be continuously triggered when called. The
// function will be triggered if called again after `wait` milliseconds.
function debounce(func, wait) {
  let timer;
  return (...args) => {
    if (timer) {
      return;
    }

    let wakeUp = () => {
      timer = null;
    };

    timer = setTimeout(wakeUp, wait);
    func.apply(this, args);
  };
}

export class _Base extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      message: {},
    };
    this.notifyContent = this.notifyContent.bind(this);
  }

  notifyContent(state) {
    this.setState(state);
  }

  componentWillUnmount() {
    this.updateTheme();
  }

  componentWillUpdate() {
    this.updateTheme();
  }

  updateTheme() {
    const bodyClassName = [
      "activity-stream",
      // If we skipped the about:welcome overlay and removed the CSS classes
      // we don't want to add them back to the Activity Stream view
      document.body.classList.contains("inline-onboarding")
        ? "inline-onboarding"
        : "",
    ]
      .filter(v => v)
      .join(" ");
    global.document.body.className = bodyClassName;
  }

  render() {
    const { props } = this;
    const { App } = props;
    const isDevtoolsEnabled = props.Prefs.values["asrouter.devtoolsEnabled"];

    if (!App.initialized) {
      return null;
    }

    return (
      <ErrorBoundary className="base-content-fallback">
        <React.Fragment>
          <BaseContent {...this.props} adminContent={this.state} />
          {isDevtoolsEnabled ? (
            <ASRouterAdmin notifyContent={this.notifyContent} />
          ) : null}
        </React.Fragment>
      </ErrorBoundary>
    );
  }
}

export class BaseContent extends React.PureComponent {
  constructor(props) {
    super(props);
    this.openPreferences = this.openPreferences.bind(this);
    this.openCustomizationMenu = this.openCustomizationMenu.bind(this);
    this.closeCustomizationMenu = this.closeCustomizationMenu.bind(this);
    this.onWindowScroll = debounce(this.onWindowScroll.bind(this), 5);
    this.setPref = this.setPref.bind(this);
    this.state = { fixedSearch: false, customizeMenuVisible: false };
  }

  componentDidMount() {
    global.addEventListener("scroll", this.onWindowScroll);
  }

  componentWillUnmount() {
    global.removeEventListener("scroll", this.onWindowScroll);
  }

  onWindowScroll() {
    const prefs = this.props.Prefs.values;
    // Show logo only if the logo is enabled and pocket is not enabled.
    const showLogo =
      prefs["logowordmark.alwaysVisible"] &&
      !(prefs["feeds.section.topstories"] && prefs["feeds.system.topstories"]);
    const SCROLL_THRESHOLD = showLogo ? 179 : 34;
    if (global.scrollY > SCROLL_THRESHOLD && !this.state.fixedSearch) {
      this.setState({ fixedSearch: true });
    } else if (global.scrollY <= SCROLL_THRESHOLD && this.state.fixedSearch) {
      this.setState({ fixedSearch: false });
    }
  }

  openPreferences() {
    this.props.dispatch(ac.OnlyToMain({ type: at.SETTINGS_OPEN }));
    this.props.dispatch(ac.UserEvent({ event: "OPEN_NEWTAB_PREFS" }));
  }

  openCustomizationMenu() {
    this.setState({ customizeMenuVisible: true });
  }

  closeCustomizationMenu() {
    this.setState({ customizeMenuVisible: false });
  }

  setPref(pref, value) {
    this.props.dispatch(ac.SetPref(pref, value));
  }

  render() {
    const { props } = this;
    const { App } = props;
    const { initialized } = App;
    const prefs = props.Prefs.values;

    // Values from experiment data
    const { prefsButtonIcon } = prefs.featureConfig || {};

    const isDiscoveryStream =
      props.DiscoveryStream.config && props.DiscoveryStream.config.enabled;
    let filteredSections = props.Sections.filter(
      section => section.id !== "topstories"
    );

    const pocketEnabled =
      prefs["feeds.section.topstories"] && prefs["feeds.system.topstories"];
    const noSectionsEnabled =
      !prefs["feeds.topsites"] &&
      !pocketEnabled &&
      filteredSections.filter(section => section.enabled).length === 0;
    const searchHandoffEnabled = prefs["improvesearch.handoffToAwesomebar"];
    const showLogo =
      prefs["logowordmark.alwaysVisible"] &&
      (!prefs["feeds.section.topstories"] ||
        (!prefs["feeds.system.topstories"] && prefs.region));

    const customizationMenuEnabled = prefs["customizationMenu.enabled"];
    const newNewtabExperienceEnabled = prefs["newNewtabExperience.enabled"];
    const canShowCustomizationMenu =
      customizationMenuEnabled || newNewtabExperienceEnabled;
    const showCustomizationMenu =
      canShowCustomizationMenu && this.state.customizeMenuVisible;
    const enabledSections = {
      topSitesEnabled: prefs["feeds.topsites"],
      pocketEnabled: prefs["feeds.section.topstories"],
      snippetsEnabled: prefs["feeds.snippets"],
      highlightsEnabled: prefs["feeds.section.highlights"],
      showSponsoredTopSitesEnabled: prefs.showSponsoredTopSites,
      showSponsoredPocketEnabled: prefs.showSponsored,
      topSitesRowsCount: prefs.topSitesRows,
    };
    const pocketRegion = prefs["feeds.system.topstories"];
    const { mayHaveSponsoredTopSites } = prefs;

    const outerClassName = [
      "outer-wrapper",
      isDiscoveryStream && pocketEnabled && "ds-outer-wrapper-search-alignment",
      isDiscoveryStream && "ds-outer-wrapper-breakpoint-override",
      prefs.showSearch &&
        this.state.fixedSearch &&
        !noSectionsEnabled &&
        "fixed-search",
      prefs.showSearch && noSectionsEnabled && "only-search",
      showLogo && "visible-logo",
    ]
      .filter(v => v)
      .join(" ");

    return (
      <div>
        {canShowCustomizationMenu ? (
          <PersonalizeButton onClick={this.openCustomizationMenu} />
        ) : (
          <PrefsButton onClick={this.openPreferences} icon={prefsButtonIcon} />
        )}
        <div className={outerClassName}>
          <main>
            {prefs.showSearch && (
              <div className="non-collapsible-section">
                <ErrorBoundary>
                  <Search
                    showLogo={noSectionsEnabled || showLogo}
                    handoffEnabled={searchHandoffEnabled}
                    {...props.Search}
                  />
                </ErrorBoundary>
              </div>
            )}
            <ASRouterUISurface
              adminContent={this.props.adminContent}
              appUpdateChannel={this.props.Prefs.values.appUpdateChannel}
              fxaEndpoint={this.props.Prefs.values.fxa_endpoint}
              dispatch={this.props.dispatch}
            />
            <div className={`body-wrapper${initialized ? " on" : ""}`}>
              {isDiscoveryStream ? (
                <ErrorBoundary className="borderless-error">
                  <DiscoveryStreamBase locale={props.App.locale} />
                </ErrorBoundary>
              ) : (
                <Sections />
              )}
            </div>
            <ConfirmDialog />
          </main>
        </div>
        {canShowCustomizationMenu && (
          <CSSTransition
            timeout={0}
            classNames="customize-animate"
            in={showCustomizationMenu}
            appear={true}
          >
            <CustomizeMenu
              onClose={this.closeCustomizationMenu}
              openPreferences={this.openPreferences}
              setPref={this.setPref}
              enabledSections={enabledSections}
              pocketRegion={pocketRegion}
              mayHaveSponsoredTopSites={mayHaveSponsoredTopSites}
              dispatch={this.props.dispatch}
            />
          </CSSTransition>
        )}
      </div>
    );
  }
}

export const Base = connect(state => ({
  App: state.App,
  Prefs: state.Prefs,
  Sections: state.Sections,
  DiscoveryStream: state.DiscoveryStream,
  Search: state.Search,
}))(_Base);
