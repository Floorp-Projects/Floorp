/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { connect } from "react-redux";
import { TopSites as OldTopSites } from "content-src/components/TopSites/TopSites";
import { TOP_SITES_MAX_SITES_PER_ROW } from "common/Reducers.jsm";
import React from "react";

export class _TopSites extends React.PureComponent {
  // Find a SPOC that doesn't already exist in User's TopSites
  getFirstAvailableSpoc(topSites, data) {
    const { spocs } = data;
    if (!spocs || spocs.length === 0) {
      return null;
    }

    const userTopSites = new Set(
      topSites.map(topSite => topSite && topSite.url)
    );

    // We "clean urls" with http in TopSiteForm.jsx
    // Spoc domains are in the format 'sponsorname.com'
    return spocs.find(
      spoc =>
        !userTopSites.has(spoc.url) &&
        !userTopSites.has(`http://${spoc.domain}`) &&
        !userTopSites.has(`https://${spoc.domain}`) &&
        !userTopSites.has(`http://www.${spoc.domain}`) &&
        !userTopSites.has(`https://www.${spoc.domain}`)
    );
  }

  // Find the first empty or unpinned index we can place the SPOC in.
  // Return -1 if no available index and we should push it at the end.
  getFirstAvailableIndex(topSites, promoAlignment) {
    if (promoAlignment === "left") {
      return topSites.findIndex(topSite => !topSite || !topSite.isPinned);
    }

    // The row isn't full so we can push it to the end of the row.
    if (topSites.length < TOP_SITES_MAX_SITES_PER_ROW) {
      return -1;
    }

    // If the row is full, we can check the row first for unpinned topsites to replace.
    // Else we can check after the row. This behavior is how unpinned topsites move while drag and drop.
    let endOfRow = TOP_SITES_MAX_SITES_PER_ROW - 1;
    for (let i = endOfRow; i >= 0; i--) {
      if (!topSites[i] || !topSites[i].isPinned) {
        return i;
      }
    }

    for (let i = endOfRow + 1; i < topSites.length; i++) {
      if (!topSites[i] || !topSites[i].isPinned) {
        return i;
      }
    }

    return -1;
  }

  insertSpocContent(TopSites, data, promoAlignment) {
    if (
      !TopSites.rows ||
      TopSites.rows.length === 0 ||
      !data.spocs ||
      data.spocs.length === 0
    ) {
      return null;
    }

    let topSites = [...TopSites.rows];
    const topSiteSpoc = this.getFirstAvailableSpoc(topSites, data);

    if (!topSiteSpoc) {
      return null;
    }

    const link = {
      customScreenshotURL: topSiteSpoc.image_src,
      type: "SPOC",
      label: topSiteSpoc.sponsor,
      title: topSiteSpoc.sponsor,
      url: topSiteSpoc.url,
      campaignId: topSiteSpoc.campaign_id,
      id: topSiteSpoc.id,
      guid: topSiteSpoc.id,
      shim: topSiteSpoc.shim,
      // For now we are assuming position based on intended position.
      // Actual position can shift based on other content.
      // We also hard code left and right to be 0 and 7.
      // We send the intended postion in the ping.
      pos: promoAlignment === "left" ? 0 : 7,
    };

    const firstAvailableIndex = this.getFirstAvailableIndex(
      topSites,
      promoAlignment
    );

    if (firstAvailableIndex === -1) {
      topSites.push(link);
    } else {
      // Normal insertion will not work since pinned topsites are in their correct index already
      // Similar logic is done to handle drag and drop with pinned topsites in TopSite.jsx

      let shiftedTopSite = topSites[firstAvailableIndex];
      let index = firstAvailableIndex + 1;

      // Shift unpinned topsites to the right by finding the next unpinned topsite to replace
      while (shiftedTopSite) {
        if (index === topSites.length) {
          topSites.push(shiftedTopSite);
          shiftedTopSite = null;
        } else if (topSites[index] && topSites[index].isPinned) {
          index += 1;
        } else {
          const nextTopSite = topSites[index];
          topSites[index] = shiftedTopSite;
          shiftedTopSite = nextTopSite;
          index += 1;
        }
      }

      topSites[firstAvailableIndex] = link;
    }

    return { ...TopSites, rows: topSites };
  }

  render() {
    const { header = {}, data, promoAlignment, TopSites } = this.props;

    const TopSitesWithSpoc =
      TopSites && data && promoAlignment
        ? this.insertSpocContent(TopSites, data, promoAlignment)
        : null;

    return (
      <div
        className={`ds-top-sites ${TopSitesWithSpoc ? "top-sites-spoc" : ""}`}
      >
        <OldTopSites
          isFixed={true}
          title={header.title}
          TopSitesWithSpoc={TopSitesWithSpoc}
        />
      </div>
    );
  }
}

export const TopSites = connect(state => ({ TopSites: state.TopSites }))(
  _TopSites
);
