/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FeatureHighlight } from "./FeatureHighlight";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import React from "react";

export function SponsoredContentHighlight({ position, dispatch }) {
  return (
    <div className="sponsored-content-highlight">
      <FeatureHighlight
        position={position}
        ariaLabel="Sponsored content supports our mission to build a better web."
        title="Sponsored content more info"
        feature="SPONSORED_CONTENT_INFO"
        dispatch={dispatch}
        message={
          <span>
            Sponsored content supports our mission to build a better web.{" "}
            <SafeAnchor
              dispatch={dispatch}
              url="https://support.mozilla.org/kb/pocket-sponsored-stories-new-tabs"
            >
              Find out how
            </SafeAnchor>
          </span>
        }
        icon={<div className="sponsored-message-icon"></div>}
        toggle={<div className="icon icon-help"></div>}
      />
    </div>
  );
}
