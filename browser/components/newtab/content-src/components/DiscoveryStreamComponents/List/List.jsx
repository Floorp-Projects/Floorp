import {actionCreators as ac} from "common/Actions.jsm";
import {connect} from "react-redux";
import React from "react";
import {truncateText} from "content-src/lib/truncate-text";

/**
 * @note exported for testing only
 */
export class ListItem extends React.PureComponent {
  // TODO performance: get feeds to send appropriately sized images rather
  // than waiting longer and scaling down on client?
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(ac.UserEvent({
        event: "CLICK",
        source: this.props.type.toUpperCase(),
        action_position: this.props.index,
      }));

      this.props.dispatch(ac.ImpressionStats({
        source: this.props.type.toUpperCase(),
        click: 0,
        tiles: [{id: this.props.id, pos: this.props.index}],
      }));
    }
  }

  render() {
    return (
      <li className="ds-list-item">
        <a className="ds-list-item-link" href={this.props.url} onClick={this.onLinkClick}>
          <div className="ds-list-item-text">
            <div className="ds-list-item-title">{this.props.title}</div>
            {this.props.excerpt && <div className="ds-list-item-excerpt">{truncateText(this.props.excerpt, 90)}</div>}
            <div className="ds-list-item-info">{this.props.domain}</div>
          </div>
          <div className="ds-list-image" style={{backgroundImage: `url(${this.props.image_src})`}} />
        </a>
      </li>
    );
  }
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

  let recMarkup = recs.slice(props.recStartingPoint,
                             props.recStartingPoint + props.items).map((rec, index) => (
    <ListItem key={`ds-list-item-${index}`}
      dispatch={props.dispatch}
      domain={rec.domain}
      excerpt={rec.excerpt}
      id={rec.id}
      image_src={rec.image_src}
      index={index}
      title={rec.title}
      type={props.type}
      url={rec.url} />)
  );

  const listStyles = [
    "ds-list",
    props.fullWidth ? "ds-list-full-width" : "",
    props.hasBorders ? "ds-list-borders" : "",
    props.hasImages ? "ds-list-images" : "",
    props.hasNumbers ? "ds-list-numbers" : "",
  ];
  return (
    <div>
      {props.header && props.header.title ? <div className="ds-header">{props.header.title}</div> : null }
      <ul className={listStyles.join(" ")}>{recMarkup}</ul>
    </div>
  );
}

_List.defaultProps = {
  recStartingPoint: 0, // Index of recommendations to start displaying from
  fullWidth: false, // Display items taking up the whole column
  hasBorders: false, // Display lines separating each item
  hasImages: false, // Display images for each item
  hasNumbers: false, // Display numbers for each item
  items: 6, // Number of stories to display.  TODO: get from endpoint
};

export const List = connect(state => ({DiscoveryStream: state.DiscoveryStream}))(_List);
