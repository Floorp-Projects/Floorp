import {actionCreators as ac} from "common/Actions.jsm";
import {connect} from "react-redux";
import {DSEmptyState} from "../DSEmptyState/DSEmptyState.jsx";
import {DSImage} from "../DSImage/DSImage.jsx";
import {DSLinkMenu} from "../DSLinkMenu/DSLinkMenu";
import {ImpressionStats} from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import React from "react";
import {SafeAnchor} from "../SafeAnchor/SafeAnchor";

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
        action_position: this.props.pos,
      }));

      this.props.dispatch(ac.ImpressionStats({
        source: this.props.type.toUpperCase(),
        click: 0,
        tiles: [{id: this.props.id, pos: this.props.pos}],
      }));
    }
  }

  render() {
    return (
      <li className={`ds-list-item${this.props.placeholder ? " placeholder" : ""}`} >
        <SafeAnchor
          className="ds-list-item-link"
          dispatch={this.props.dispatch}
          onLinkClick={!this.props.placeholder ? this.onLinkClick : undefined}
          url={this.props.url}>
          <div className="ds-list-item-text">
            <div>
              <div className="ds-list-item-title clamp">{this.props.title}</div>
              {this.props.excerpt && <div className="ds-list-item-excerpt clamp">{this.props.excerpt}</div>}
            </div>
            <p>
              {this.props.context && (
                <span>
                  <span className="ds-list-item-context clamp">{this.props.context}</span>
                  <br />
                </span>
              )}
              <span className="ds-list-item-info clamp">{this.props.domain}</span>
            </p>
          </div>
          <DSImage extraClassNames="ds-list-image" source={this.props.image_src} rawSource={this.props.raw_image_src} />
          <ImpressionStats
            campaignId={this.props.campaignId}
            rows={[{id: this.props.id, pos: this.props.pos}]}
            dispatch={this.props.dispatch}
            source={this.props.type} />
        </SafeAnchor>
        {!this.props.placeholder && <DSLinkMenu
          id={this.props.id}
          index={this.props.pos}
          dispatch={this.props.dispatch}
          intl={this.props.intl}
          url={this.props.url}
          title={this.props.title}
          source={this.props.source}
          type={this.props.type}
          pocket_id={this.props.pocket_id}
          bookmarkGuid={this.props.bookmarkGuid} />}
      </li>
    );
  }
}

export const PlaceholderListItem = props => <ListItem placeholder={true} />;

/**
 * @note exported for testing only
 */
export function _List(props) {
  const renderList = () => {
    const recs = props.data.recommendations.slice(props.recStartingPoint, props.recStartingPoint + props.items);
    const recMarkup = [];

    for (let index = 0; index < props.items; index++) {
      const rec = recs[index];
      recMarkup.push(!rec || rec.placeholder ? (
        <PlaceholderListItem key={`ds-list-item-${index}`} />
      ) : (
        <ListItem key={`ds-list-item-${index}`}
        dispatch={props.dispatch}
        campaignId={rec.campaign_id}
        domain={rec.domain}
        excerpt={rec.excerpt}
        id={rec.id}
        image_src={rec.image_src}
        raw_image_src={rec.raw_image_src}
        pos={rec.pos}
        title={rec.title}
        context={rec.context}
        type={props.type}
        url={rec.url}
        pocket_id={rec.pocket_id}
        bookmarkGuid={rec.bookmarkGuid} />
      ));
    }

    const listStyles = [
      "ds-list",
      props.fullWidth ? "ds-list-full-width" : "",
      props.hasBorders ? "ds-list-borders" : "",
      props.hasImages ? "ds-list-images" : "",
      props.hasNumbers ? "ds-list-numbers" : "",
    ];

    return (
      <ul className={listStyles.join(" ")}>{recMarkup}</ul>
    );
  };

  const feed = props.data;
  if (!feed || !feed.recommendations) {
    return null;
  }

  // Handle the case where a user has dismissed all recommendations
  const isEmpty = feed.recommendations.length === 0;

  return (
    <div>
      {props.header && props.header.title ? <div className="ds-header">{props.header.title}</div> : null }
      {isEmpty ?
        <div className="ds-list empty"><DSEmptyState /></div> :
        renderList()
      }
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
