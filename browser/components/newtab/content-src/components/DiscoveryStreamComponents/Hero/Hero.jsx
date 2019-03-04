import {actionCreators as ac} from "common/Actions.jsm";
import {DSCard} from "../DSCard/DSCard.jsx";
import {ImpressionStats} from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import {List} from "../List/List.jsx";
import React from "react";
import {SafeAnchor} from "../SafeAnchor/SafeAnchor";

export class Hero extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(ac.UserEvent({
        event: "CLICK",
        source: this.props.type.toUpperCase(),
        action_position: this.heroRec.pos,
      }));

      this.props.dispatch(ac.ImpressionStats({
        source: this.props.type.toUpperCase(),
        click: 0,
        tiles: [{id: this.heroRec.id, pos: this.heroRec.pos}],
      }));
    }
  }

  render() {
    const {data} = this.props;

    // Handle a render before feed has been fetched by displaying nothing
    if (!data || !data.recommendations) {
      return (
        <div />
      );
    }

    let [heroRec, ...otherRecs] = data.recommendations.slice(0, this.props.items);
    this.heroRec = heroRec;

    let cards = otherRecs.map((rec, index) => (
      <DSCard
        campaignId={rec.campaign_id}
        key={`dscard-${index}`}
        image_src={rec.image_src}
        title={rec.title}
        url={rec.url}
        id={rec.id}
        pos={rec.pos}
        type={this.props.type}
        dispatch={this.props.dispatch}
        context={rec.context}
        source={rec.domain} />
    ));

    let list = (
      <List
        recStartingPoint={1}
        data={data}
        hasImages={true}
        hasBorders={this.props.border === `border`}
        items={this.props.items - 1}
        type={`Hero`} />
    );

    return (
      <div>
        <div className="ds-header">{this.props.title}</div>
        <div className={`ds-hero ds-hero-${this.props.border}`}>
          <SafeAnchor url={heroRec.url} className="wrapper" onLinkClick={this.onLinkClick}>
            <div className="img-wrapper">
              <div className="img" style={{backgroundImage: `url(${heroRec.image_src})`}} />
            </div>
            <div className="meta">
              <div className="header-and-excerpt">
                <header>{heroRec.title}</header>
                <p className="excerpt">{heroRec.excerpt}</p>
              </div>
              {heroRec.context ? (
                <p className="context">{heroRec.context}</p>
              ) : (
                <p className="source">{heroRec.domain}</p>
              )}
            </div>
            <ImpressionStats
              campaignId={heroRec.campaignId}
              rows={[{id: heroRec.id, pos: heroRec.pos}]}
              dispatch={this.props.dispatch}
              source={this.props.type} />
          </SafeAnchor>
          <div className={`${this.props.subComponentType}`}>
            { this.props.subComponentType === `cards` ? cards : list }
          </div>
        </div>
      </div>
    );
  }
}

Hero.defaultProps = {
  data: {},
  border: `border`,
  items: 1, // Number of stories to display
};
