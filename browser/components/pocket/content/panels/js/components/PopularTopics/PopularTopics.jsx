/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import TelemetryLink from "../TelemetryLink/TelemetryLink";

function PopularTopics(props) {
  return (
    <ul className="stp_popular_topics">
      {props.topics?.map((topic, position) => (
        <li key={`item-${topic.topic}`} className="stp_popular_topic">
          <TelemetryLink
            className="stp_popular_topic_link"
            href={`https://${props.pockethost}/explore/${topic.topic}?${props.utmParams}`}
            source={props.source}
            position={position}
          >
            {topic.title}
          </TelemetryLink>
        </li>
      ))}
    </ul>
  );
}

export default PopularTopics;
