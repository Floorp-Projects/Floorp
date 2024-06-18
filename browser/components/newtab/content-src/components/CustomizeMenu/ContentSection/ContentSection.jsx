/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { actionCreators as ac } from "common/Actions.mjs";
import { SafeAnchor } from "../../DiscoveryStreamComponents/SafeAnchor/SafeAnchor";
import { WallpapersSection } from "../../WallpapersSection/WallpapersSection";
import { WallpaperCategories } from "../../WallpapersSection/WallpaperCategories";

export class ContentSection extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onPreferenceSelect = this.onPreferenceSelect.bind(this);

    // Refs are necessary for dynamically measuring drawer heights for slide animations
    this.topSitesDrawerRef = React.createRef();
    this.pocketDrawerRef = React.createRef();
  }

  inputUserEvent(eventSource, status) {
    this.props.dispatch(
      ac.UserEvent({
        event: "PREF_CHANGED",
        source: eventSource,
        value: { status, menu_source: "CUSTOMIZE_MENU" },
      })
    );
  }

  onPreferenceSelect(e) {
    // eventSource: TOP_SITES | TOP_STORIES | HIGHLIGHTS | WEATHER
    const { preference, eventSource } = e.target.dataset;
    let value;
    if (e.target.nodeName === "SELECT") {
      value = parseInt(e.target.value, 10);
    } else if (e.target.nodeName === "INPUT") {
      value = e.target.checked;
      if (eventSource) {
        this.inputUserEvent(eventSource, value);
      }
    } else if (e.target.nodeName === "MOZ-TOGGLE") {
      value = e.target.pressed;
      if (eventSource) {
        this.inputUserEvent(eventSource, value);
      }
    }
    this.props.setPref(preference, value);
  }

  componentDidMount() {
    this.setDrawerMargins();
  }

  componentDidUpdate() {
    this.setDrawerMargins();
  }

  setDrawerMargins() {
    this.setDrawerMargin(
      `TOP_SITES`,
      this.props.enabledSections.topSitesEnabled
    );
    this.setDrawerMargin(
      `TOP_STORIES`,
      this.props.enabledSections.pocketEnabled
    );
  }

  setDrawerMargin(drawerID, isOpen) {
    let drawerRef;

    if (drawerID === `TOP_SITES`) {
      drawerRef = this.topSitesDrawerRef.current;
    } else if (drawerID === `TOP_STORIES`) {
      drawerRef = this.pocketDrawerRef.current;
    } else {
      return;
    }

    let drawerHeight;

    if (drawerRef) {
      drawerHeight = parseFloat(window.getComputedStyle(drawerRef)?.height);

      if (isOpen) {
        drawerRef.style.marginTop = `0`;
      } else {
        drawerRef.style.marginTop = `-${drawerHeight}px`;
      }
    }
  }

  render() {
    const {
      enabledSections,
      mayHaveSponsoredTopSites,
      pocketRegion,
      mayHaveSponsoredStories,
      mayHaveRecentSaves,
      mayHaveWeather,
      openPreferences,
      spocMessageVariant,
      wallpapersEnabled,
      wallpapersV2Enabled,
      activeWallpaper,
      setPref,
    } = this.props;
    const {
      topSitesEnabled,
      pocketEnabled,
      highlightsEnabled,
      weatherEnabled,
      showSponsoredTopSitesEnabled,
      showSponsoredPocketEnabled,
      showRecentSavesEnabled,
      topSitesRowsCount,
    } = enabledSections;

    return (
      <div className="home-section">
        {!wallpapersV2Enabled && wallpapersEnabled && (
          <div className="wallpapers-section">
            <WallpapersSection
              setPref={setPref}
              activeWallpaper={activeWallpaper}
            />
          </div>
        )}
        {wallpapersV2Enabled && (
          <div className="wallpapers-section">
            <WallpaperCategories
              setPref={setPref}
              activeWallpaper={activeWallpaper}
            />
          </div>
        )}
        <div id="shortcuts-section" className="section">
          <moz-toggle
            id="shortcuts-toggle"
            pressed={topSitesEnabled || null}
            onToggle={this.onPreferenceSelect}
            data-preference="feeds.topsites"
            data-eventSource="TOP_SITES"
            data-l10n-id="newtab-custom-shortcuts-toggle"
            data-l10n-attrs="label, description"
          />
          <div>
            <div className="more-info-top-wrapper">
              <div className="more-information" ref={this.topSitesDrawerRef}>
                <select
                  id="row-selector"
                  className="selector"
                  name="row-count"
                  data-preference="topSitesRows"
                  value={topSitesRowsCount}
                  onChange={this.onPreferenceSelect}
                  disabled={!topSitesEnabled}
                  aria-labelledby="custom-shortcuts-title"
                >
                  <option
                    value="1"
                    data-l10n-id="newtab-custom-row-selector"
                    data-l10n-args='{"num": 1}'
                  />
                  <option
                    value="2"
                    data-l10n-id="newtab-custom-row-selector"
                    data-l10n-args='{"num": 2}'
                  />
                  <option
                    value="3"
                    data-l10n-id="newtab-custom-row-selector"
                    data-l10n-args='{"num": 3}'
                  />
                  <option
                    value="4"
                    data-l10n-id="newtab-custom-row-selector"
                    data-l10n-args='{"num": 4}'
                  />
                </select>
                {mayHaveSponsoredTopSites && (
                  <div className="check-wrapper" role="presentation">
                    <input
                      id="sponsored-shortcuts"
                      className="sponsored-checkbox"
                      disabled={!topSitesEnabled}
                      checked={showSponsoredTopSitesEnabled}
                      type="checkbox"
                      onChange={this.onPreferenceSelect}
                      data-preference="showSponsoredTopSites"
                      data-eventSource="SPONSORED_TOP_SITES"
                    />
                    <label
                      className="sponsored"
                      htmlFor="sponsored-shortcuts"
                      data-l10n-id="newtab-custom-sponsored-sites"
                    />
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>

        {pocketRegion && (
          <div id="pocket-section" className="section">
            <label className="switch">
              <moz-toggle
                id="pocket-toggle"
                pressed={pocketEnabled || null}
                onToggle={this.onPreferenceSelect}
                aria-describedby="custom-pocket-subtitle"
                data-preference="feeds.section.topstories"
                data-eventSource="TOP_STORIES"
                data-l10n-id="newtab-custom-stories-toggle"
                data-l10n-attrs="label, description"
              />
            </label>
            <div>
              {(mayHaveSponsoredStories || mayHaveRecentSaves) && (
                <div className="more-info-pocket-wrapper">
                  <div className="more-information" ref={this.pocketDrawerRef}>
                    {mayHaveSponsoredStories && (
                      <div className="check-wrapper" role="presentation">
                        <input
                          id="sponsored-pocket"
                          className="sponsored-checkbox"
                          disabled={!pocketEnabled}
                          checked={showSponsoredPocketEnabled}
                          type="checkbox"
                          onChange={this.onPreferenceSelect}
                          data-preference="showSponsored"
                          data-eventSource="POCKET_SPOCS"
                        />
                        <label
                          className="sponsored"
                          htmlFor="sponsored-pocket"
                          data-l10n-id="newtab-custom-pocket-sponsored"
                        />
                      </div>
                    )}
                    {mayHaveRecentSaves && (
                      <div className="check-wrapper" role="presentation">
                        <input
                          id="recent-saves-pocket"
                          className="sponsored-checkbox"
                          disabled={!pocketEnabled}
                          checked={showRecentSavesEnabled}
                          type="checkbox"
                          onChange={this.onPreferenceSelect}
                          data-preference="showRecentSaves"
                          data-eventSource="POCKET_RECENT_SAVES"
                        />
                        <label
                          className="sponsored"
                          htmlFor="recent-saves-pocket"
                          data-l10n-id="newtab-custom-pocket-show-recent-saves"
                        />
                      </div>
                    )}
                  </div>
                </div>
              )}
            </div>
          </div>
        )}

        <div id="recent-section" className="section">
          <label className="switch">
            <moz-toggle
              id="highlights-toggle"
              pressed={highlightsEnabled || null}
              onToggle={this.onPreferenceSelect}
              data-preference="feeds.section.highlights"
              data-eventSource="HIGHLIGHTS"
              data-l10n-id="newtab-custom-recent-toggle"
              data-l10n-attrs="label, description"
            />
          </label>
        </div>

        {mayHaveWeather && (
          <div id="weather-section" className="section">
            <label className="switch">
              <moz-toggle
                id="weather-toggle"
                pressed={weatherEnabled || null}
                onToggle={this.onPreferenceSelect}
                data-preference="showWeather"
                data-eventSource="WEATHER"
                data-l10n-id="newtab-custom-weather-toggle"
                data-l10n-attrs="label, description"
              />
            </label>
          </div>
        )}

        {pocketRegion &&
          mayHaveSponsoredStories &&
          spocMessageVariant === "variant-c" && (
            <div className="sponsored-content-info">
              <div className="icon icon-help"></div>
              <div>
                Sponsored content supports our mission to build a better web.{" "}
                <SafeAnchor
                  dispatch={this.props.dispatch}
                  url="https://support.mozilla.org/kb/pocket-sponsored-stories-new-tabs"
                >
                  Find out how
                </SafeAnchor>
              </div>
            </div>
          )}

        <span className="divider" role="separator"></span>

        <div>
          <button
            id="settings-link"
            className="external-link"
            onClick={openPreferences}
            data-l10n-id="newtab-custom-settings"
          />
        </div>
      </div>
    );
  }
}
