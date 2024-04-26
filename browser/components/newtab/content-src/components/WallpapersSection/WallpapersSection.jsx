/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { connect } from "react-redux";

export class _WallpapersSection extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
    this.handleReset = this.handleReset.bind(this);
    this.prefersHighContrastQuery = null;
    this.prefersDarkQuery = null;
  }

  componentDidMount() {
    this.prefersHighContrastQuery = globalThis.matchMedia(
      "(forced-colors: active)"
    );
    this.prefersDarkQuery = globalThis.matchMedia(
      "(prefers-color-scheme: dark)"
    );
    [this.prefersHighContrastQuery, this.prefersDarkQuery].forEach(
      colorTheme => {
        colorTheme.addEventListener("change", this.handleReset);
      }
    );
  }

  componentWillUnmount() {
    [this.prefersHighContrastQuery, this.prefersDarkQuery].forEach(
      colorTheme => {
        colorTheme.removeEventListener("change", this.handleReset);
      }
    );
  }

  handleChange(event) {
    const { id } = event.target;
    this.props.setPref("newtabWallpapers.wallpaper", id);
  }

  handleReset() {
    this.props.setPref("newtabWallpapers.wallpaper", "");
  }

  render() {
    const { wallpaperList } = this.props.Wallpapers;
    const { activeWallpaper } = this.props;
    return (
      <div>
        <fieldset className="wallpaper-list">
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
