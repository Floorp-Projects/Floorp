/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { MIN_RICH_FAVICON_SIZE, TOP_SITES_SOURCE } from "./TopSitesConstants";
import { CollapsibleSection } from "content-src/components/CollapsibleSection/CollapsibleSection";
import { ComponentPerfTimer } from "content-src/components/ComponentPerfTimer/ComponentPerfTimer";
import { connect } from "react-redux";
import { ModalOverlayWrapper } from "content-src/components/ModalOverlay/ModalOverlay";
import React from "react";
import { SearchShortcutsForm } from "./SearchShortcutsForm";
import { TOP_SITES_MAX_SITES_PER_ROW } from "common/Reducers.sys.mjs";
import { TopSiteForm } from "./TopSiteForm";
import { TopSiteList } from "./TopSite";

function topSiteIconType(link) {
  if (link.customScreenshotURL) {
    return "custom_screenshot";
  }
  if (link.tippyTopIcon || link.faviconRef === "tippytop") {
    return "tippytop";
  }
  if (link.faviconSize >= MIN_RICH_FAVICON_SIZE) {
    return "rich_icon";
  }
  if (link.screenshot) {
    return "screenshot";
  }
  return "no_image";
}

/**
 * Iterates through TopSites and counts types of images.
 * @param acc Accumulator for reducer.
 * @param topsite Entry in TopSites.
 */
function countTopSitesIconsTypes(topSites) {
  const countTopSitesTypes = (acc, link) => {
    acc[topSiteIconType(link)]++;
    return acc;
  };

  return topSites.reduce(countTopSitesTypes, {
    custom_screenshot: 0,
    screenshot: 0,
    tippytop: 0,
    rich_icon: 0,
    no_image: 0,
  });
}

export class _TopSites extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onEditFormClose = this.onEditFormClose.bind(this);
    this.onSearchShortcutsFormClose =
      this.onSearchShortcutsFormClose.bind(this);
  }

  /**
   * Dispatch session statistics about the quality of TopSites icons and pinned count.
   */
  _dispatchTopSitesStats() {
    const topSites = this._getVisibleTopSites().filter(
      topSite => topSite !== null && topSite !== undefined
    );
    const topSitesIconsStats = countTopSitesIconsTypes(topSites);
    const topSitesPinned = topSites.filter(site => !!site.isPinned).length;
    const searchShortcuts = topSites.filter(
      site => !!site.searchTopSite
    ).length;
    // Dispatch telemetry event with the count of TopSites images types.
    this.props.dispatch(
      ac.AlsoToMain({
        type: at.SAVE_SESSION_PERF_DATA,
        data: {
          topsites_icon_stats: topSitesIconsStats,
          topsites_pinned: topSitesPinned,
          topsites_search_shortcuts: searchShortcuts,
        },
      })
    );
  }

  /**
   * Return the TopSites that are visible based on prefs and window width.
   */
  _getVisibleTopSites() {
    // We hide 2 sites per row when not in the wide layout.
    let sitesPerRow = TOP_SITES_MAX_SITES_PER_ROW;
    // $break-point-widest = 1072px (from _variables.scss)
    if (!globalThis.matchMedia(`(min-width: 1072px)`).matches) {
      sitesPerRow -= 2;
    }
    return this.props.TopSites.rows.slice(
      0,
      this.props.TopSitesRows * sitesPerRow
    );
  }

  componentDidUpdate() {
    this._dispatchTopSitesStats();
  }

  componentDidMount() {
    this._dispatchTopSitesStats();
  }

  onEditFormClose() {
    this.props.dispatch(
      ac.UserEvent({
        source: TOP_SITES_SOURCE,
        event: "TOP_SITES_EDIT_CLOSE",
      })
    );
    this.props.dispatch({ type: at.TOP_SITES_CANCEL_EDIT });
  }

  onSearchShortcutsFormClose() {
    this.props.dispatch(
      ac.UserEvent({
        source: TOP_SITES_SOURCE,
        event: "SEARCH_EDIT_CLOSE",
      })
    );
    this.props.dispatch({ type: at.TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL });
  }

  render() {
    const { props } = this;
    const { editForm, showSearchShortcutsForm } = props.TopSites;
    const extraMenuOptions = ["AddTopSite"];
    const colors = props.Prefs.values["newNewtabExperience.colors"];

    if (props.Prefs.values["improvesearch.topSiteSearchShortcuts"]) {
      extraMenuOptions.push("AddSearchShortcut");
    }

    return (
      <ComponentPerfTimer
        id="topsites"
        initialized={props.TopSites.initialized}
        dispatch={props.dispatch}
      >
        <CollapsibleSection
          className="top-sites"
          id="topsites"
          title={props.title || { id: "newtab-section-header-topsites" }}
          hideTitle={true}
          extraMenuOptions={extraMenuOptions}
          showPrefName="feeds.topsites"
          eventSource={TOP_SITES_SOURCE}
          collapsed={false}
          isFixed={props.isFixed}
          isFirst={props.isFirst}
          isLast={props.isLast}
          dispatch={props.dispatch}
        >
          <TopSiteList
            TopSites={props.TopSites}
            TopSitesRows={props.TopSitesRows}
            dispatch={props.dispatch}
            topSiteIconType={topSiteIconType}
            colors={colors}
          />
          <div className="edit-topsites-wrapper">
            {editForm && (
              <div className="edit-topsites">
                <ModalOverlayWrapper
                  unstyled={true}
                  onClose={this.onEditFormClose}
                  innerClassName="modal"
                >
                  <TopSiteForm
                    site={props.TopSites.rows[editForm.index]}
                    onClose={this.onEditFormClose}
                    dispatch={this.props.dispatch}
                    {...editForm}
                  />
                </ModalOverlayWrapper>
              </div>
            )}
            {showSearchShortcutsForm && (
              <div className="edit-search-shortcuts">
                <ModalOverlayWrapper
                  unstyled={true}
                  onClose={this.onSearchShortcutsFormClose}
                  innerClassName="modal"
                >
                  <SearchShortcutsForm
                    TopSites={props.TopSites}
                    onClose={this.onSearchShortcutsFormClose}
                    dispatch={this.props.dispatch}
                  />
                </ModalOverlayWrapper>
              </div>
            )}
          </div>
        </CollapsibleSection>
      </ComponentPerfTimer>
    );
  }
}

export const TopSites = connect(state => ({
  TopSites: state.TopSites,
  Prefs: state.Prefs,
  TopSitesRows: state.Prefs.values.topSitesRows,
}))(_TopSites);
