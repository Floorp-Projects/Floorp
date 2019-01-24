import {actionCreators as ac} from "common/Actions.jsm";
import {connect} from "react-redux";
import React from "react";

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
            <div className="ds-list-item-title">
              <b>
                {this.props.title}
              </b>
            </div>
            <div className="ds-list-item-info">
              {`${this.props.domain} · TODO:Topic`}
            </div>
          </div>
          <img className="ds-list-image" src={this.props.image_src} />
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

  let recMarkup = recs.slice(0, props.items).map((rec, index) => (
    <ListItem {...rec} key={`ds-list-item-${index}`} index={index} type={props.type} dispatch={props.dispatch} />)
  );

  const listStyles = [
    "ds-list",
    props.hasImages ? "ds-list-images" : "",
    props.hasNumbers ? "ds-list-numbers" : "",
  ];
  return (
    <div>
      {props.header && props.header.title ? <div className="ds-header">{props.header.title}</div> : null }
      <hr className="ds-list-border" />
      <ul className={listStyles.join(" ")}>{recMarkup}</ul>
    </div>
  );
}

_List.defaultProps = {
  hasImages: false, // Display images for each item
  hasNumbers: false, // Display numbers for each item
  items: 6, // Number of stories to display.  TODO: get from endpoint
};

export const List = connect(state => ({DiscoveryStream: state.DiscoveryStream}))(_List);
