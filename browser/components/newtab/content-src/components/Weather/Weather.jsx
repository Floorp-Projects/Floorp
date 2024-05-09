/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { connect } from "react-redux";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import React from "react";

export class _Weather extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      contextMenuKeyboard: false,
      showContextMenu: false,
      url: "https://example.com",
    };
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onUpdate = this.onUpdate.bind(this);
  }

  openContextMenu(isKeyBoard) {
    if (this.props.onUpdate) {
      this.props.onUpdate(true);
    }
    this.setState({
      showContextMenu: true,
      contextMenuKeyboard: isKeyBoard,
    });
  }

  onClick(event) {
    event.preventDefault();
    this.openContextMenu(false, event);
  }

  onKeyDown(event) {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      this.openContextMenu(true, event);
    }
  }

  onUpdate(showContextMenu) {
    if (this.props.onUpdate) {
      this.props.onUpdate(showContextMenu);
    }
    this.setState({ showContextMenu });
  }

  render() {
    // Check if weather should be rendered
    const isWeatherEnabled = this.props.Prefs.values["system.showWeather"];

    if (!isWeatherEnabled) {
      return false;
    }

    const { showContextMenu } = this.state;

    const WEATHER_SUGGESTION = this.props.Weather.suggestions?.[0];

    const {
      className,
      index,
      dispatch,
      eventSource,
      shouldSendImpressionStats,
    } = this.props;
    const { props } = this;
    const isContextMenuOpen = this.state.activeCard === index;

    const outerClassName = [
      "weather",
      className,
      isContextMenuOpen && "active",
      props.placeholder && "placeholder",
    ]
      .filter(v => v)
      .join(" ");

    const showDetailedView =
      this.props.Prefs.values["weather.display"] === "detailed";

    // Note: The temperature units/display options will become secondary menu items
    const WEATHER_SOURCE_CONTEXT_MENU_OPTIONS = [
      ...(this.props.Prefs.values["weather.locationSearchEnabled"]
        ? ["ChangeWeatherLocation"]
        : []),
      ...(this.props.Prefs.values["weather.temperatureUnits"] === "f"
        ? ["ChangeTempUnitCelsius"]
        : ["ChangeTempUnitFahrenheit"]),
      ...(this.props.Prefs.values["weather.display"] === "simple"
        ? ["ChangeWeatherDisplayDetailed"]
        : ["ChangeWeatherDisplaySimple"]),
      "HideWeather",
      "OpenLearnMoreURL",
    ];

    // Only return the widget if we have data. Otherwise, show error state
    if (WEATHER_SUGGESTION) {
      return (
        <div className={outerClassName}>
          <div className="weatherCard">
            <a
              data-l10n-id="newtab-weather-see-forecast"
              data-l10n-args='{"provider": "AccuWeather"}'
              href={WEATHER_SUGGESTION.forecast.url}
              className="weatherInfoLink"
            >
              <div className="weatherIconCol">
                <span
                  className={`weatherIcon iconId${WEATHER_SUGGESTION.current_conditions.icon_id}`}
                />
              </div>
              <div className="weatherText">
                <div className="weatherForecastRow">
                  <span className="weatherTemperature">
                    {
                      WEATHER_SUGGESTION.current_conditions.temperature[
                        this.props.Prefs.values["weather.temperatureUnits"]
                      ]
                    }
                    &deg;{this.props.Prefs.values["weather.temperatureUnits"]}
                  </span>
                </div>
                <div className="weatherCityRow">
                  <span className="weatherCity">
                    {WEATHER_SUGGESTION.city_name}
                  </span>
                </div>
                {showDetailedView ? (
                  <div className="weatherDetailedSummaryRow">
                    <div className="weatherHighLowTemps">
                      {/* Low Forecasted Temperature */}
                      <span>
                        {
                          WEATHER_SUGGESTION.forecast.high[
                            this.props.Prefs.values["weather.temperatureUnits"]
                          ]
                        }
                        &deg;
                        {this.props.Prefs.values["weather.temperatureUnits"]}
                      </span>
                      {/* Spacer / Bullet */}
                      <span>&bull;</span>
                      {/* Low Forecasted Temperature */}
                      <span>
                        {
                          WEATHER_SUGGESTION.forecast.low[
                            this.props.Prefs.values["weather.temperatureUnits"]
                          ]
                        }
                        &deg;
                        {this.props.Prefs.values["weather.temperatureUnits"]}
                      </span>
                    </div>
                    <span className="weatherTextSummary">
                      {WEATHER_SUGGESTION.current_conditions.summary}
                    </span>
                  </div>
                ) : null}
              </div>
            </a>
            <div className="weatherButtonContextMenuWrapper">
              <button
                aria-haspopup="true"
                onKeyDown={this.onKeyDown}
                onClick={this.onClick}
                data-l10n-id="newtab-menu-section-tooltip"
                className="weatherButtonContextMenu"
              >
                {showContextMenu ? (
                  <LinkMenu
                    dispatch={dispatch}
                    index={index}
                    source={eventSource}
                    onUpdate={this.onUpdate}
                    options={WEATHER_SOURCE_CONTEXT_MENU_OPTIONS}
                    site={{
                      url: "https://support.mozilla.org/kb/customize-items-on-firefox-new-tab-page",
                    }}
                    link="https://support.mozilla.org/kb/customize-items-on-firefox-new-tab-page"
                    shouldSendImpressionStats={shouldSendImpressionStats}
                  />
                ) : null}
              </button>
            </div>
          </div>
          <span
            data-l10n-id="newtab-weather-sponsored"
            data-l10n-args='{"provider": "AccuWeather"}'
            className="weatherSponsorText"
          ></span>
        </div>
      );
    }

    return (
      <div className={outerClassName}>
        <div className="weatherNotAvailable">
          <span className="icon icon-small-spacer icon-info-critical" />{" "}
          <span data-l10n-id="newtab-weather-error-not-available"></span>
        </div>
      </div>
    );
  }
}

export const Weather = connect(state => ({
  Weather: state.Weather,
  Prefs: state.Prefs,
}))(_Weather);
