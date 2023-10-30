/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import { TOP_SITES_SOURCE } from "../TopSites/TopSitesConstants";
import React from "react";

const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";

// Per analytical requirement, we set the minimal intersection ratio to
// 0.5, and an impression is identified when the wrapped item has at least
// 50% visibility.
//
// This constant is exported for unit test
export const INTERSECTION_RATIO = 0.5;

/**
 * Impression wrapper for Discovery Stream related React components.
 *
 * It makses use of the Intersection Observer API to detect the visibility,
 * and relies on page visibility to ensure the impression is reported
 * only when the component is visible on the page.
 *
 * Note:
 *   * This wrapper used to be used either at the individual card level,
 *     or by the card container components.
 *     It is now only used for individual card level.
 *   * Each impression will be sent only once as soon as the desired
 *     visibility is detected
 *   * Batching is not yet implemented, hence it might send multiple
 *     impression pings separately
 */
export class ImpressionStats extends React.PureComponent {
  // This checks if the given cards are the same as those in the last impression ping.
  // If so, it should not send the same impression ping again.
  _needsImpressionStats(cards) {
    if (
      !this.impressionCardGuids ||
      this.impressionCardGuids.length !== cards.length
    ) {
      return true;
    }

    for (let i = 0; i < cards.length; i++) {
      if (cards[i].id !== this.impressionCardGuids[i]) {
        return true;
      }
    }

    return false;
  }

  _dispatchImpressionStats() {
    const { props } = this;
    const cards = props.rows;

    if (this.props.flightId) {
      this.props.dispatch(
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_SPOC_IMPRESSION,
          data: { flightId: this.props.flightId },
        })
      );

      // Record sponsored topsites impressions if the source is `TOP_SITES_SOURCE`.
      if (this.props.source === TOP_SITES_SOURCE) {
        for (const card of cards) {
          this.props.dispatch(
            ac.OnlyToMain({
              type: at.TOP_SITES_SPONSORED_IMPRESSION_STATS,
              data: {
                type: "impression",
                tile_id: card.id,
                source: "newtab",
                advertiser: card.advertiser,
                // Keep the 0-based position, can be adjusted by the telemetry
                // sender if necessary.
                position: card.pos,
              },
            })
          );
        }
      }
    }

    if (this._needsImpressionStats(cards)) {
      props.dispatch(
        ac.DiscoveryStreamImpressionStats({
          source: props.source.toUpperCase(),
          window_inner_width: window.innerWidth,
          window_inner_height: window.innerHeight,
          tiles: cards.map(link => ({
            id: link.id,
            pos: link.pos,
            type: this.props.flightId ? "spoc" : "organic",
            ...(link.shim ? { shim: link.shim } : {}),
            recommendation_id: link.recommendation_id,
          })),
        })
      );
      this.impressionCardGuids = cards.map(link => link.id);
    }
  }

  // This checks if the given cards are the same as those in the last loaded content ping.
  // If so, it should not send the same loaded content ping again.
  _needsLoadedContent(cards) {
    if (
      !this.loadedContentGuids ||
      this.loadedContentGuids.length !== cards.length
    ) {
      return true;
    }

    for (let i = 0; i < cards.length; i++) {
      if (cards[i].id !== this.loadedContentGuids[i]) {
        return true;
      }
    }

    return false;
  }

  _dispatchLoadedContent() {
    const { props } = this;
    const cards = props.rows;

    if (this._needsLoadedContent(cards)) {
      props.dispatch(
        ac.DiscoveryStreamLoadedContent({
          source: props.source.toUpperCase(),
          tiles: cards.map(link => ({ id: link.id, pos: link.pos })),
        })
      );
      this.loadedContentGuids = cards.map(link => link.id);
    }
  }

  setImpressionObserverOrAddListener() {
    const { props } = this;

    if (!props.dispatch) {
      return;
    }

    if (props.document.visibilityState === VISIBLE) {
      // Send the loaded content ping once the page is visible.
      this._dispatchLoadedContent();
      this.setImpressionObserver();
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
          // Send the loaded content ping once the page is visible.
          this._dispatchLoadedContent();
          this.setImpressionObserver();
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

  /**
   * Set an impression observer for the wrapped component. It makes use of
   * the Intersection Observer API to detect if the wrapped component is
   * visible with a desired ratio, and only sends impression if that's the case.
   *
   * See more details about Intersection Observer API at:
   * https://developer.mozilla.org/en-US/docs/Web/API/Intersection_Observer_API
   */
  setImpressionObserver() {
    const { props } = this;

    if (!props.rows.length) {
      return;
    }

    this._handleIntersect = entries => {
      if (
        entries.some(
          entry =>
            entry.isIntersecting &&
            entry.intersectionRatio >= INTERSECTION_RATIO
        )
      ) {
        this._dispatchImpressionStats();
        this.impressionObserver.unobserve(this.refs.impression);
      }
    };

    const options = { threshold: INTERSECTION_RATIO };
    this.impressionObserver = new props.IntersectionObserver(
      this._handleIntersect,
      options
    );
    this.impressionObserver.observe(this.refs.impression);
  }

  componentDidMount() {
    if (this.props.rows.length) {
      this.setImpressionObserverOrAddListener();
    }
  }

  componentWillUnmount() {
    if (this._handleIntersect && this.impressionObserver) {
      this.impressionObserver.unobserve(this.refs.impression);
    }
    if (this._onVisibilityChange) {
      this.props.document.removeEventListener(
        VISIBILITY_CHANGE_EVENT,
        this._onVisibilityChange
      );
    }
  }

  render() {
    return (
      <div ref={"impression"} className="impression-observer">
        {this.props.children}
      </div>
    );
  }
}

ImpressionStats.defaultProps = {
  IntersectionObserver: global.IntersectionObserver,
  document: global.document,
  rows: [],
  source: "",
};
