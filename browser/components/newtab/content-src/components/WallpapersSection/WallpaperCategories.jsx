/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { connect } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { CSSTransition } from "react-transition-group";

export class _WallpaperCategories extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
    this.handleReset = this.handleReset.bind(this);
    this.handleCategory = this.handleCategory.bind(this);
    this.handleBack = this.handleBack.bind(this);
    this.prefersHighContrastQuery = null;
    this.prefersDarkQuery = null;
    this.state = {
      activeCategory: null,
      activeCategoryFluentID: null,
    };
  }

  componentDidMount() {
    this.prefersDarkQuery = globalThis.matchMedia(
      "(prefers-color-scheme: dark)"
    );
  }

  handleChange(event) {
    const { id } = event.target;
    this.props.setPref("newtabWallpapers.wallpaper-light", id);
    this.props.setPref("newtabWallpapers.wallpaper-dark", id);
    // Setting this now so when we remove v1 we don't have to migrate v1 values.
    this.props.setPref("newtabWallpapers.wallpaper", id);
    this.handleUserEvent(at.WALLPAPER_CLICK, {
      selected_wallpaper: id,
      hadPreviousWallpaper: !!this.props.activeWallpaper,
    });
  }
  handleReset() {
    this.props.setPref("newtabWallpapers.wallpaper-light", "");
    this.props.setPref("newtabWallpapers.wallpaper-dark", "");
    // Setting this now so when we remove v1 we don't have to migrate v1 values.
    this.props.setPref("newtabWallpapers.wallpaper", "");
    this.handleUserEvent(at.WALLPAPER_CLICK, {
      selected_wallpaper: "none",
      hadPreviousWallpaper: !!this.props.activeWallpaper,
    });
  }

  handleCategory = event => {
    this.setState({ activeCategory: event.target.id });
    this.handleUserEvent(at.WALLPAPER_CATEGORY_CLICK, event.target.id);

    let fluent_id;
    switch (event.target.id) {
      case "photographs":
        fluent_id = "newtab-wallpaper-category-title-photographs";
        break;
      case "abstract":
        fluent_id = "newtab-wallpaper-category-title-abstract";
        break;
      case "solid-colors":
        fluent_id = "newtab-wallpaper-category-title-colors";
    }

    this.setState({ activeCategoryFluentID: fluent_id });
  };

  handleBack() {
    this.setState({ activeCategory: null });
  }

  // Record user interaction when changing wallpaper and reseting wallpaper to default
  handleUserEvent(type, data) {
    this.props.dispatch(ac.OnlyToMain({ type, data }));
  }

  render() {
    const prefs = this.props.Prefs.values;
    const { wallpaperList, categories } = this.props.Wallpapers;
    const { activeWallpaper } = this.props;
    const { activeCategory } = this.state;
    const { activeCategoryFluentID } = this.state;
    const filteredWallpapers = wallpaperList.filter(
      wallpaper => wallpaper.category === activeCategory
    );
    let categorySectionClassname = "category wallpaper-list";
    if (prefs["newtabWallpapers.v2.enabled"]) {
      categorySectionClassname += " ignore-color-mode";
    }

    return (
      <div>
        <div className="category-header">
          <h2 data-l10n-id="newtab-wallpaper-title"></h2>
          <button
            className="wallpapers-reset"
            onClick={this.handleReset}
            data-l10n-id="newtab-wallpaper-reset"
          />
        </div>
        <fieldset className="category-list">
          {categories.map(category => {
            const firstWallpaper = wallpaperList.find(
              wallpaper => wallpaper.category === category
            );
            const title = firstWallpaper ? firstWallpaper.title : "";
            const solid_color = firstWallpaper
              ? firstWallpaper.solid_color
              : "";

            let fluent_id;
            switch (category) {
              case "photographs":
                fluent_id = "newtab-wallpaper-category-title-photographs";
                break;
              case "abstract":
                fluent_id = "newtab-wallpaper-category-title-abstract";
                break;
              case "solid-colors":
                fluent_id = "newtab-wallpaper-category-title-colors";
            }

            const style = {
              backgroundColor: solid_color || "transparent",
            };

            return (
              <div key={category}>
                <input
                  id={category}
                  style={style}
                  onClick={this.handleCategory}
                  className={`wallpaper-input ${title}`}
                />
                <label htmlFor={category} data-l10n-id={fluent_id}>
                  {fluent_id}
                </label>
              </div>
            );
          })}
        </fieldset>

        <CSSTransition
          in={!!activeCategory}
          timeout={300}
          classNames="wallpaper-list"
          unmountOnExit={true}
        >
          <section className={categorySectionClassname}>
            <button
              className="arrow-button"
              data-l10n-id={activeCategoryFluentID}
              onClick={this.handleBack}
            />
            <fieldset>
              {filteredWallpapers.map(
                ({ title, theme, fluent_id, solid_color }) => {
                  const style = {
                    backgroundColor: solid_color || "transparent",
                  };
                  return (
                    <>
                      <input
                        onChange={this.handleChange}
                        style={style}
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
                }
              )}
            </fieldset>
          </section>
        </CSSTransition>
      </div>
    );
  }
}

export const WallpaperCategories = connect(state => {
  return {
    Wallpapers: state.Wallpapers,
    Prefs: state.Prefs,
  };
})(_WallpaperCategories);
