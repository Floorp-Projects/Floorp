/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import React from "react";
import { TOP_SITES_SOURCE } from "./TopSitesConstants";

export class SelectableSearchShortcut extends React.PureComponent {
  render() {
    const { shortcut, selected } = this.props;
    const imageStyle = { backgroundImage: `url("${shortcut.tippyTopIcon}")` };
    return (
      <div className="top-site-outer search-shortcut">
        <input
          type="checkbox"
          id={shortcut.keyword}
          name={shortcut.keyword}
          checked={selected}
          onChange={this.props.onChange}
        />
        <label htmlFor={shortcut.keyword}>
          <div className="top-site-inner">
            <span>
              <div className="tile">
                <div
                  className="top-site-icon rich-icon"
                  style={imageStyle}
                  data-fallback="@"
                />
                <div className="top-site-icon search-topsite" />
              </div>
              <div className="title">
                <span dir="auto">{shortcut.keyword}</span>
              </div>
            </span>
          </div>
        </label>
      </div>
    );
  }
}

export class SearchShortcutsForm extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
    this.onCancelButtonClick = this.onCancelButtonClick.bind(this);
    this.onSaveButtonClick = this.onSaveButtonClick.bind(this);

    // clone the shortcuts and add them to the state so we can add isSelected property
    const shortcuts = [];
    const { rows, searchShortcuts } = props.TopSites;
    searchShortcuts.forEach(shortcut => {
      shortcuts.push({
        ...shortcut,
        isSelected: !!rows.find(
          row =>
            row &&
            row.isPinned &&
            row.searchTopSite &&
            row.label === shortcut.keyword
        ),
      });
    });
    this.state = { shortcuts };
  }

  handleChange(event) {
    const { target } = event;
    const { name, checked } = target;
    this.setState(prevState => {
      const shortcuts = prevState.shortcuts.slice();
      let shortcut = shortcuts.find(({ keyword }) => keyword === name);
      shortcut.isSelected = checked;
      return { shortcuts };
    });
  }

  onCancelButtonClick(ev) {
    ev.preventDefault();
    this.props.onClose();
  }

  onSaveButtonClick(ev) {
    ev.preventDefault();

    // Check if there were any changes and act accordingly
    const { rows } = this.props.TopSites;
    const pinQueue = [];
    const unpinQueue = [];
    this.state.shortcuts.forEach(shortcut => {
      const alreadyPinned = rows.find(
        row =>
          row &&
          row.isPinned &&
          row.searchTopSite &&
          row.label === shortcut.keyword
      );
      if (shortcut.isSelected && !alreadyPinned) {
        pinQueue.push(this._searchTopSite(shortcut));
      } else if (!shortcut.isSelected && alreadyPinned) {
        unpinQueue.push({
          url: alreadyPinned.url,
          searchVendor: shortcut.shortURL,
        });
      }
    });

    // Tell the feed to do the work.
    this.props.dispatch(
      ac.OnlyToMain({
        type: at.UPDATE_PINNED_SEARCH_SHORTCUTS,
        data: {
          addedShortcuts: pinQueue,
          deletedShortcuts: unpinQueue,
        },
      })
    );

    // Send the Telemetry pings.
    pinQueue.forEach(shortcut => {
      this.props.dispatch(
        ac.UserEvent({
          source: TOP_SITES_SOURCE,
          event: "SEARCH_EDIT_ADD",
          value: { search_vendor: shortcut.searchVendor },
        })
      );
    });
    unpinQueue.forEach(shortcut => {
      this.props.dispatch(
        ac.UserEvent({
          source: TOP_SITES_SOURCE,
          event: "SEARCH_EDIT_DELETE",
          value: { search_vendor: shortcut.searchVendor },
        })
      );
    });

    this.props.onClose();
  }

  _searchTopSite(shortcut) {
    return {
      url: shortcut.url,
      searchTopSite: true,
      label: shortcut.keyword,
      searchVendor: shortcut.shortURL,
    };
  }

  render() {
    return (
      <form className="topsite-form">
        <div className="search-shortcuts-container">
          <h3
            className="section-title grey-title"
            data-l10n-id="newtab-topsites-add-search-engine-header"
          />
          <div>
            {this.state.shortcuts.map(shortcut => (
              <SelectableSearchShortcut
                key={shortcut.keyword}
                shortcut={shortcut}
                selected={shortcut.isSelected}
                onChange={this.handleChange}
              />
            ))}
          </div>
        </div>
        <section className="actions">
          <button
            className="cancel"
            type="button"
            onClick={this.onCancelButtonClick}
            data-l10n-id="newtab-topsites-cancel-button"
          />
          <button
            className="done"
            type="submit"
            onClick={this.onSaveButtonClick}
            data-l10n-id="newtab-topsites-save-button"
          />
        </section>
      </form>
    );
  }
}
