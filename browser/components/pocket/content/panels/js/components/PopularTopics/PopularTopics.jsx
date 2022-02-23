/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

function PopularTopics(props) {
  return (
    <ul className="stp_popular_topics">
      {props.topics?.map(topic => (
        <li key={`item-${topic.topic}`} className="stp_popular_topic">
          <a
            className="stp_popular_topic_link"
            href={`https://${props.pockethost}/explore/${topic.topic}?utm_source=${props.utmsource}`}
          >
            {topic.title}
          </a>
        </li>
      ))}
    </ul>
  );
}

export default PopularTopics;
