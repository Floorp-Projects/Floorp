/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { connect } from "react-redux";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import { LocationSearch } from "content-src/components/Weather/LocationSearch";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import React from "react";

const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

export class _Weather extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      contextMenuKeyboard: false,
      showContextMenu: false,
      url: "https://example.com",
      impressionSeen: false,
      errorSeen: false,
    };
    this.setImpressionRef = element => {
      this.impressionElement = element;
    };
    this.setErrorRef = element => {
      this.errorElement = element;
    };
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onUpdate = this.onUpdate.bind(this);
    this.onProviderClick = this.onProviderClick.bind(this);
  }

  componentDidMount() {
    const { props } = this;

    if (!props.dispatch) {
      return;
    }

    if (props.document.visibilityState === VISIBLE) {
      // Setup the impression observer once the page is visible.
      this.setImpressionObservers();
    } else {
      // We should only ever send the latest impression stats ping, so remove any
      // older listeners.
      if (this._onVisibilityChange) {
        props.document.removeEventListener(
          VISIBILITY_CHANGE_EVENT,
          this._onVisibilityChange
        );
      }

      this._onVisibilityChange = () => {
        if (props.document.visibilityState === VISIBLE) {
          // Setup the impression observer once the page is visible.
          this.setImpressionObservers();
          props.document.removeEventListener(
            VISIBILITY_CHANGE_EVENT,
            this._onVisibilityChange
          );
        }
      };
      props.document.addEventListener(
        VISIBILITY_CHANGE_EVENT,
        this._onVisibilityChange
      );
    }
  }

  componentWillUnmount() {
    // Remove observers on unmount
    if (this.observer && this.impressionElement) {
      this.observer.unobserve(this.impressionElement);
    }
    if (this.observer && this.errorElement) {
      this.observer.unobserve(this.errorElement);
    }
    if (this._onVisibilityChange) {
      this.props.document.removeEventListener(
        VISIBILITY_CHANGE_EVENT,
        this._onVisibilityChange
      );
    }
  }

  setImpressionObservers() {
    if (this.impressionElement) {
      this.observer = new IntersectionObserver(this.onImpression.bind(this));
      this.observer.observe(this.impressionElement);
    }
    if (this.errorElement) {
      this.observer = new IntersectionObserver(this.onError.bind(this));
      this.observer.observe(this.errorElement);
    }
  }

  onImpression(entries) {
    if (this.state) {
      const entry = entries.find(e => e.isIntersecting);

      if (entry) {
        if (this.impressionElement) {
          this.observer.unobserve(this.impressionElement);
        }

        this.props.dispatch(
          ac.OnlyToMain({
            type: at.WEATHER_IMPRESSION,
          })
        );

        // Stop observing since element has been seen
        this.setState({
          impressionSeen: true,
        });
      }
    }
  }

  onError(entries) {
    if (this.state) {
      const entry = entries.find(e => e.isIntersecting);

      if (entry) {
        if (this.errorElement) {
          this.observer.unobserve(this.errorElement);
        }

        this.props.dispatch(
          ac.OnlyToMain({
            type: at.WEATHER_LOAD_ERROR,
          })
        );

        // Stop observing since element has been seen
        this.setState({
          errorSeen: true,
        });
      }
    }
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

  onProviderClick() {
    this.props.dispatch(
      ac.OnlyToMain({
        type: at.WEATHER_OPEN_PROVIDER_URL,
        data: {
          source: "WEATHER",
        },
      })
    );
  }

  render() {
    // Check if weather should be rendered
    const isWeatherEnabled = this.props.Prefs.values["system.showWeather"];

    if (!isWeatherEnabled || !this.props.Weather.initialized) {
      return false;
    }

    const { showContextMenu } = this.state;

    const { props } = this;

    const {
      className,
      index,
      dispatch,
      eventSource,
      shouldSendImpressionStats,
      Prefs,
      Weather,
    } = props;

    const WEATHER_SUGGESTION = Weather.suggestions?.[0];
    const isContextMenuOpen = this.state.activeCard === index;

    const outerClassName = [
      "weather",
      className,
      isContextMenuOpen && !Weather.searchActive && "active",
      props.placeholder && "placeholder",
      Weather.searchActive && "search",
    ]
      .filter(v => v)
      .join(" ");

    const showDetailedView = Prefs.values["weather.display"] === "detailed";

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

    if (Weather.searchActive) {
      return <LocationSearch outerClassName={outerClassName} />;
    } else if (WEATHER_SUGGESTION) {
      return (
        <div ref={this.setImpressionRef} className={outerClassName}>
          <div className="weatherCard">
            <a
              data-l10n-id="newtab-weather-see-forecast"
              data-l10n-args='{"provider": "AccuWeather"}'
              href={WEATHER_SUGGESTION.forecast.url}
              className="weatherInfoLink"
              onClick={this.onProviderClick}
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
                    {Weather.locationData.city}
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
          <span className="weatherSponsorText">
            <span
              data-l10n-id="newtab-weather-sponsored"
              data-l10n-args='{"provider": "AccuWeather"}'
            ></span>
          </span>
        </div>
      );
    }

    return (
      <div ref={this.setErrorRef} className={outerClassName}>
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
  IntersectionObserver: globalThis.IntersectionObserver,
  document: globalThis.document,
}))(_Weather);
