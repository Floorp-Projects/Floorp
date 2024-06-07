/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FeatureHighlight } from "./FeatureHighlight";
import React, { useCallback, useEffect, useState, useRef } from "react";
import { useSelector } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";

const INTERSECTION_RATIO = 1;
const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
const WALLPAPER_HIGHLIGHT_DISMISSED_PREF =
  "newtabWallpapers.highlightDismissed";

export function WallpaperFeatureHighlight({
  position,
  dispatch,
  windowObj = globalThis,
}) {
  const [highlightVisibilityTimeoutID, setHighlightVisibilityTimeoutID] =
    useState("");
  const heightElement = useRef(null);
  const prefs = useSelector(state => state.Prefs.values);
  const highlightHeaderText = prefs["newtabWallpapers.highlightHeaderText"];
  const highlightContentText = prefs["newtabWallpapers.highlightContentText"];
  const highlightCtaText = prefs["newtabWallpapers.highlightCtaText"];

  // Event triggered by the CTA click event in the FeatureHighlight component.
  const onToggleClick = useCallback(() => {
    dispatch(ac.SetPref(WALLPAPER_HIGHLIGHT_DISMISSED_PREF, true));
    dispatch(
      ac.OnlyToMain({ type: at.WALLPAPERS_FEATURE_HIGHLIGHT_CTA_CLICKED })
    );
  }, [dispatch]);

  // Event triggered by the onDismiss click event in the FeatureHighlight component.
  const onDismissCallback = useCallback(() => {
    dispatch(ac.SetPref(WALLPAPER_HIGHLIGHT_DISMISSED_PREF, true));
    dispatch(
      ac.OnlyToMain({ type: at.WALLPAPERS_FEATURE_HIGHLIGHT_DISMISSED })
    );
  }, [dispatch]);

  // Event triggered by the onOutsideClick click event in the FeatureHighlight component.
  const onOutsideClickCallback = () => {
    clearTimeout(highlightVisibilityTimeoutID);
  };

  // Update the counter everytime the Wallpaper Highlight is viewed for more than 3 seconds
  useEffect(() => {
    const options = { threshold: INTERSECTION_RATIO };
    const intersectionObserver = new windowObj.IntersectionObserver(entries => {
      if (
        entries.some(
          entry =>
            entry.isIntersecting &&
            entry.intersectionRatio >= INTERSECTION_RATIO
        )
      ) {
        intersectionObserver.unobserve(heightElement.current);

        // Set the timeout ID so that it can be cleared independently
        // if there is an outside click detected
        setHighlightVisibilityTimeoutID(
          setTimeout(() => {
            dispatch(
              ac.OnlyToMain({ type: at.WALLPAPERS_FEATURE_HIGHLIGHT_SEEN })
            );
          }, 3000)
        );
      }
    }, options);

    const onVisibilityChange = () => {
      intersectionObserver.observe(heightElement.current);
      windowObj.document.removeEventListener(
        VISIBILITY_CHANGE_EVENT,
        onVisibilityChange
      );
    };

    if (heightElement.current) {
      if (windowObj.document.visibilityState === VISIBLE) {
        intersectionObserver.observe(heightElement.current);
      } else {
        windowObj.document.addEventListener(
          VISIBILITY_CHANGE_EVENT,
          onVisibilityChange
        );
      }
    }

    return () => {
      intersectionObserver?.disconnect();
      windowObj.document.removeEventListener(
        VISIBILITY_CHANGE_EVENT,
        onVisibilityChange
      );
    };
  }, [dispatch, windowObj]);

  let header = (
    <span
      className="highlightHeader"
      data-l10n-id="newtab-wallpaper-feature-highlight-header"
    ></span>
  );

  let content = (
    <span
      className="highlightContent"
      data-l10n-id="newtab-wallpaper-feature-highlight-content"
    ></span>
  );

  let button = (
    <button
      onClick={onToggleClick}
      data-l10n-id="newtab-wallpaper-feature-highlight-button"
    ></button>
  );

  if (highlightHeaderText) {
    header = <span className="highlightHeader">{highlightHeaderText}</span>;
  }
  if (highlightContentText) {
    content = <span className="highlightContent">{highlightContentText}</span>;
  }
  if (highlightCtaText) {
    button = <button onClick={onToggleClick}>{highlightCtaText}</button>;
  }

  return (
    <div className="wallpaper-feature-highlight" ref={heightElement}>
      <FeatureHighlight
        position={position}
        data-l10n-id="feature-highlight-wallpaper"
        feature="FEATURE_HIGHLIGHT_WALLPAPER"
        dispatch={dispatch}
        message={
          <div className="wallpaper-feature-highlight-content">
            {header}
            {content}
            {button}
          </div>
        }
        icon={<div className="customize-icon"></div>}
        toggle={<div className="icon icon-help"></div>}
        openedOverride={true}
        showButtonIcon={false}
        dismissCallback={onDismissCallback}
        outsideClickCallback={onOutsideClickCallback}
      />
    </div>
  );
}
