import {connect} from "react-redux";
import React from "react";

/**
 * @note exported for testing only
 */
export function ListItem(props) {
  // TODO performance: get feeds to send appropriately sized images rather
  // than waiting longer and scaling down on client?
  return (
    <li className="ds-list-item">
      <a className="ds-list-item-link" href={props.url}>
        <div className="ds-list-item-text">
          <div className="ds-list-item-title">
            <b>
              {props.title}
            </b>
          </div>
          <div className="ds-list-item-info">
            {`${props.domain} · TODO:Topic`}
          </div>
        </div>

        <img className="ds-list-image" src={props.image_src} />
      </a>

    </li>
  );
}

/**
 * @note exported for testing only
 */
export function _List(props) {
  const feed = props.DiscoveryStream.feeds[props.feed.url];

  if (!feed || !feed.data || !feed.data.recommendations) {
    return null;
  }

  const recs = feed.data.recommendations;

  let recMarkup = recs.slice(0, props.items).map((rec, index) => (
    <ListItem {...rec} key={`ds-list-item-$index`} />)
  );

  return (
    <div>
      <h3 className="ds-list-title">{props.header && props.header.title}</h3>
      <hr className="ds-list-border" />
      <ul className="ds-list">{recMarkup}</ul>
    </div>
  );
}

_List.defaultProps = {
  items: 6, // Number of stories to display.  TODO: get from endpoint
};

export const List = connect(state => ({DiscoveryStream: state.DiscoveryStream}))(_List);
