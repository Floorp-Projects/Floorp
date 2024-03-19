/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useCallback, useRef, useEffect } from "react";
import { actionCreators as ac } from "common/Actions.mjs";

export function FeatureHighlight({
  message,
  icon,
  toggle,
  position = "top-left",
  title,
  ariaLabel,
  source = "FEATURE_HIGHLIGHT_DEFAULT",
  dispatch = () => {},
  windowObj = global,
}) {
  const [opened, setOpened] = useState(false);
  const ref = useRef(null);

  useEffect(() => {
    const handleOutsideClick = e => {
      if (!ref?.current?.contains(e.target)) {
        setOpened(false);
      }
    };

    windowObj.document.addEventListener("click", handleOutsideClick);
    return () => {
      windowObj.document.removeEventListener("click", handleOutsideClick);
    };
  }, [windowObj]);

  const onToggleClick = useCallback(() => {
    setOpened(!opened);
    dispatch(
      ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source,
      })
    );
  }, [dispatch, source, opened]);

  const openedClassname = opened ? `opened` : `closed`;
  return (
    <div ref={ref} className="feature-highlight">
      <button
        title={title}
        aria-haspopup="true"
        aria-label={ariaLabel}
        className="toggle-button"
        onClick={onToggleClick}
      >
        {toggle}
      </button>
      <div className={`feature-highlight-modal ${position} ${openedClassname}`}>
        <div className="message-icon">{icon}</div>
        <p>{message}</p>
        <button
          title="Dismiss"
          aria-label="Close sponsored content more info popup"
          className="icon icon-dismiss"
          onClick={() => setOpened(false)}
        ></button>
      </div>
    </div>
  );
}
