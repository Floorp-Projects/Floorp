/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { connect } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";

export class _WallpapersSection extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
    this.handleReset = this.handleReset.bind(this);
    this.prefersHighContrastQuery = null;
    this.prefersDarkQuery = null;
  }

  componentDidMount() {
    this.prefersDarkQuery = globalThis.matchMedia(
      "(prefers-color-scheme: dark)"
    );
  }

  handleChange(event) {
    const { id } = event.target;
    const prefs = this.props.Prefs.values;
    const colorMode = this.prefersDarkQuery?.matches ? "dark" : "light";
    if (prefs["newtabWallpapers.v2.enabled"]) {
      // If we don't care about color mode, set both to the same wallpaper.
      this.props.setPref(`newtabWallpapers.wallpaper-dark`, id);
      this.props.setPref(`newtabWallpapers.wallpaper-light`, id);
    } else {
      this.props.setPref(`newtabWallpapers.wallpaper-${colorMode}`, id);
      // bug 1892095
      if (
        prefs["newtabWallpapers.wallpaper-dark"] === "" &&
        colorMode === "light"
      ) {
        this.props.setPref(
          "newtabWallpapers.wallpaper-dark",
          id.replace("light", "dark")
        );
      }

      if (
        prefs["newtabWallpapers.wallpaper-light"] === "" &&
        colorMode === "dark"
      ) {
        this.props.setPref(
          `newtabWallpapers.wallpaper-light`,
          id.replace("dark", "light")
        );
      }
    }
    this.handleUserEvent({
      selected_wallpaper: id,
      hadPreviousWallpaper: !!this.props.activeWallpaper,
    });
  }

  handleReset() {
    const prefs = this.props.Prefs.values;
    const colorMode = this.prefersDarkQuery?.matches ? "dark" : "light";
    if (prefs["newtabWallpapers.v2.enabled"]) {
      this.props.setPref("newtabWallpapers.wallpaper-light", "");
      this.props.setPref("newtabWallpapers.wallpaper-dark", "");
    } else {
      this.props.setPref(`newtabWallpapers.wallpaper-${colorMode}`, "");
    }
    this.handleUserEvent({
      selected_wallpaper: "none",
      hadPreviousWallpaper: !!this.props.activeWallpaper,
    });
  }

  // Record user interaction when changing wallpaper and reseting wallpaper to default
  handleUserEvent(data) {
    this.props.dispatch(
      ac.OnlyToMain({
        type: at.WALLPAPER_CLICK,
        data,
      })
    );
  }

  render() {
    const { wallpaperList } = this.props.Wallpapers;
    const { activeWallpaper } = this.props;
    const prefs = this.props.Prefs.values;
    let fieldsetClassname = `wallpaper-list`;
    if (prefs["newtabWallpapers.v2.enabled"]) {
      fieldsetClassname += " ignore-color-mode";
    }
    return (
      <div>
        <fieldset className={fieldsetClassname}>
          {wallpaperList.map(({ title, theme, fluent_id }) => {
            return (
              <>
                <input
                  onChange={this.handleChange}
                  type="radio"
                  name={`wallpaper-${title}`}
                  id={title}
                  value={title}
                  checked={title === activeWallpaper}
                  aria-checked={title === activeWallpaper}
                  className={`wallpaper-input theme-${theme} ${title}`}
                />
                <label
                  htmlFor={title}
                  className="sr-only"
                  data-l10n-id={fluent_id}
                >
                  {fluent_id}
                </label>
              </>
            );
          })}
        </fieldset>
        <button
          className="wallpapers-reset"
          onClick={this.handleReset}
          data-l10n-id="newtab-wallpaper-reset"
        />
      </div>
    );
  }
}

export const WallpapersSection = connect(state => {
  return {
    Wallpapers: state.Wallpapers,
    Prefs: state.Prefs,
  };
})(_WallpapersSection);
