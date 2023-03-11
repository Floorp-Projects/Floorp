/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.sys.mjs";
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
 * Impression wrapper for a TopSite tile.
 *
 * It makses use of the Intersection Observer API to detect the visibility,
 * and relies on page visibility to ensure the impression is reported
 * only when the component is visible on the page.
 */
export class TopSiteImpressionWrapper extends React.PureComponent {
  _dispatchImpressionStats() {
    const { actionType, tile } = this.props;
    if (!actionType) {
      return;
    }

    this.props.dispatch(
      ac.OnlyToMain({
        type: actionType,
        data: {
          type: "impression",
          ...tile,
        },
      })
    );
  }

  setImpressionObserverOrAddListener() {
    const { props } = this;

    if (!props.dispatch) {
      return;
    }

    if (props.document.visibilityState === VISIBLE) {
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

    if (!props.tile) {
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
        this.impressionObserver.unobserve(this.refs.topsite_impression_wrapper);
      }
    };

    const options = { threshold: INTERSECTION_RATIO };
    this.impressionObserver = new props.IntersectionObserver(
      this._handleIntersect,
      options
    );
    this.impressionObserver.observe(this.refs.topsite_impression_wrapper);
  }

  componentDidMount() {
    if (this.props.tile) {
      this.setImpressionObserverOrAddListener();
    }
  }

  componentWillUnmount() {
    if (this._handleIntersect && this.impressionObserver) {
      this.impressionObserver.unobserve(this.refs.topsite_impression_wrapper);
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
      <div
        ref={"topsite_impression_wrapper"}
        className="topsite-impression-observer"
      >
        {this.props.children}
      </div>
    );
  }
}

TopSiteImpressionWrapper.defaultProps = {
  IntersectionObserver: global.IntersectionObserver,
  document: global.document,
  actionType: null,
  tile: null,
};
