/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

function PopularTopicsLegacy(props) {
  return (
    <>
      <h3 data-l10n-id="pocket-panel-home-explore-popular-topics"></h3>
      <ul>
        {props.topics.map(item => (
          <li key={`item-${item.topic}`}>
            <a
              className="pkt_ext_topic"
              href={`https://${props.pockethost}/explore/${item.topic}?utm_source=${props.utmsource}`}
            >
              {item.title}
              <span className="pkt_ext_chevron_right"></span>
            </a>
          </li>
        ))}
      </ul>
      <a
        className="pkt_ext_discover"
        href={`https://${props.pockethost}/explore?utm_source=${props.utmsource}`}
        data-l10n-id="pocket-panel-home-discover-more"
      />
    </>
  );
}

export default PopularTopicsLegacy;
