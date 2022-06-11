/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { actionCreators as ac } from "common/Actions.jsm";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { ImpressionStats } from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import { connect } from "react-redux";

export function _TopicsWidget(props) {
  const { id, source, position, DiscoveryStream, dispatch } = props;

  const { utmCampaign, utmContent, utmSource } = DiscoveryStream.experimentData;

  let queryParams = `?utm_source=${utmSource}`;
  if (utmCampaign && utmContent) {
    queryParams += `&utm_content=${utmContent}&utm_campaign=${utmCampaign}`;
  }

  const topics = [
    { label: "Technology", name: "technology" },
    { label: "Science", name: "science" },
    { label: "Self-Improvement", name: "self-improvement" },
    { label: "Travel", name: "travel" },
    { label: "Career", name: "career" },
    { label: "Entertainment", name: "entertainment" },
    { label: "Food", name: "food" },
    { label: "Health", name: "health" },
    {
      label: "Must-Reads",
      name: "must-reads",
      url: `https://getpocket.com/collections${queryParams}`,
    },
  ];

  function onLinkClick(topic, positionInCard) {
    if (dispatch) {
      dispatch(
        ac.UserEvent({
          event: "CLICK",
          source,
          action_position: position,
          value: {
            card_type: "topics_widget",
            topic,
            ...(positionInCard || positionInCard === 0
              ? { position_in_card: positionInCard }
              : {}),
          },
        })
      );
      dispatch(
        ac.ImpressionStats({
          source,
          click: 0,
          window_inner_width: props.windowObj.innerWidth,
          window_inner_height: props.windowObj.innerHeight,
          tiles: [
            {
              id,
              pos: position,
            },
          ],
        })
      );
    }
  }

  function mapTopicItem(topic, index) {
    return (
      <li
        key={topic.name}
        className={topic.overflow ? "ds-topics-widget-list-overflow-item" : ""}
      >
        <SafeAnchor
          url={
            topic.url ||
            `https://getpocket.com/explore/${topic.name}${queryParams}`
          }
          dispatch={dispatch}
          onLinkClick={() => onLinkClick(topic.name, index)}
        >
          {topic.label}
        </SafeAnchor>
      </li>
    );
  }

  return (
    <div className="ds-topics-widget">
      <header className="ds-topics-widget-header">Popular Topics</header>
      <hr />
      <div className="ds-topics-widget-list-container">
        <ul>{topics.map(mapTopicItem)}</ul>
      </div>
      <SafeAnchor
        className="ds-topics-widget-button button primary"
        url={`https://getpocket.com/explore${queryParams}`}
        dispatch={dispatch}
        onLinkClick={() => onLinkClick("more-topics")}
      >
        More Topics
      </SafeAnchor>
      <ImpressionStats
        dispatch={dispatch}
        rows={[
          {
            id,
            pos: position,
          },
        ]}
        source={source}
      />
    </div>
  );
}

_TopicsWidget.defaultProps = {
  windowObj: window, // Added to support unit tests
};

export const TopicsWidget = connect(state => ({
  DiscoveryStream: state.DiscoveryStream,
}))(_TopicsWidget);
