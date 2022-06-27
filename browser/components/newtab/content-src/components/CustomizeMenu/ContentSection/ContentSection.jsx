/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { actionCreators as ac } from "common/Actions.jsm";

export class ContentSection extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onPreferenceSelect = this.onPreferenceSelect.bind(this);
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
    let prefName = e.target.getAttribute("preference");
    const eventSource = e.target.getAttribute("eventSource");
    let value;
    if (e.target.nodeName === "SELECT") {
      value = parseInt(e.target.value, 10);
    } else if (e.target.nodeName === "INPUT") {
      value = e.target.checked;
      if (eventSource) {
        this.inputUserEvent(eventSource, value);
      }
    }
    this.props.setPref(prefName, value);
  }

  render() {
    const {
      topSitesEnabled,
      pocketEnabled,
      highlightsEnabled,
      showSponsoredTopSitesEnabled,
      showSponsoredPocketEnabled,
      showRecentSavesEnabled,
      recentSavesExperiment,
      topSitesRowsCount,
    } = this.props.enabledSections;

    return (
      <div className="home-section">
        <div id="shortcuts-section" className="section">
          <label className="switch">
            <input
              id="shortcuts-toggle"
              checked={topSitesEnabled}
              type="checkbox"
              onChange={this.onPreferenceSelect}
              preference="feeds.topsites"
              aria-labelledby="custom-shortcuts-title"
              aria-describedby="custom-shortcuts-subtitle"
              eventSource="TOP_SITES"
            />
            <span className="slider" role="presentation"></span>
          </label>
          <div>
            <h2 id="custom-shortcuts-title" className="title">
              <label
                htmlFor="shortcuts-toggle"
                data-l10n-id="newtab-custom-shortcuts-title"
              ></label>
            </h2>
            <p
              id="custom-shortcuts-subtitle"
              className="subtitle"
              data-l10n-id="newtab-custom-shortcuts-subtitle"
            ></p>
            <div
              className={`more-info-top-wrapper ${
                topSitesEnabled ? "" : "shrink"
              }`}
            >
              <div
                className={`more-information ${
                  topSitesEnabled ? "expand" : "shrink"
                }`}
              >
                <select
                  id="row-selector"
                  className="selector"
                  name="row-count"
                  preference="topSitesRows"
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
                {this.props.mayHaveSponsoredTopSites && (
                  <div className="check-wrapper" role="presentation">
                    <input
                      id="sponsored-shortcuts"
                      className="sponsored-checkbox"
                      disabled={!topSitesEnabled}
                      checked={showSponsoredTopSitesEnabled}
                      type="checkbox"
                      onChange={this.onPreferenceSelect}
                      preference="showSponsoredTopSites"
                      eventSource="SPONSORED_TOP_SITES"
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

        {this.props.pocketRegion && (
          <div id="pocket-section" className="section">
            <label className="switch">
              <input
                id="pocket-toggle"
                checked={pocketEnabled}
                type="checkbox"
                onChange={this.onPreferenceSelect}
                preference="feeds.section.topstories"
                aria-labelledby="custom-pocket-title"
                aria-describedby="custom-pocket-subtitle"
                eventSource="TOP_STORIES"
              />
              <span className="slider" role="presentation"></span>
            </label>
            <div>
              <h2 id="custom-pocket-title" className="title">
                <label
                  htmlFor="pocket-toggle"
                  data-l10n-id="newtab-custom-pocket-title"
                ></label>
              </h2>
              <p
                id="custom-pocket-subtitle"
                className="subtitle"
                data-l10n-id="newtab-custom-pocket-subtitle"
              />
              {this.props.mayHaveSponsoredStories && (
                <div
                  className={`more-info-pocket-wrapper ${
                    pocketEnabled ? "" : "shrink"
                  }`}
                >
                  <div
                    className={`more-information ${
                      pocketEnabled ? "expand" : "shrink"
                    }`}
                  >
                    <div className="check-wrapper" role="presentation">
                      <input
                        id="sponsored-pocket"
                        className="sponsored-checkbox"
                        disabled={!pocketEnabled}
                        checked={showSponsoredPocketEnabled}
                        type="checkbox"
                        onChange={this.onPreferenceSelect}
                        preference="showSponsored"
                        eventSource="POCKET_SPOCS"
                      />
                      <label
                        className="sponsored"
                        htmlFor="sponsored-pocket"
                        data-l10n-id="newtab-custom-pocket-sponsored"
                      />
                    </div>
                    {recentSavesExperiment && (
                      <div className="check-wrapper" role="presentation">
                        <input
                          id="recent-saves-pocket"
                          className="sponsored-checkbox"
                          disabled={!pocketEnabled}
                          checked={showRecentSavesEnabled}
                          type="checkbox"
                          onChange={this.onPreferenceSelect}
                          preference="showRecentSaves"
                          eventSource="POCKET_RECENT_SAVES"
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
            <input
              id="highlights-toggle"
              checked={highlightsEnabled}
              type="checkbox"
              onChange={this.onPreferenceSelect}
              preference="feeds.section.highlights"
              eventSource="HIGHLIGHTS"
              aria-labelledby="custom-recent-title"
              aria-describedby="custom-recent-subtitle"
            />
            <span className="slider" role="presentation"></span>
          </label>
          <div>
            <h2 id="custom-recent-title" className="title">
              <label
                htmlFor="highlights-toggle"
                data-l10n-id="newtab-custom-recent-title"
              ></label>
            </h2>

            <p
              id="custom-recent-subtitle"
              className="subtitle"
              data-l10n-id="newtab-custom-recent-subtitle"
            />
          </div>
        </div>

        <span className="divider" role="separator"></span>

        <div>
          <button
            id="settings-link"
            className="external-link"
            onClick={this.props.openPreferences}
            data-l10n-id="newtab-custom-settings"
          />
        </div>
      </div>
    );
  }
}
