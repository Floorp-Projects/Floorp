import {DSCard, PlaceholderDSCard} from "../DSCard/DSCard.jsx";
import {actionCreators as ac} from "common/Actions.jsm";
import {DSEmptyState} from "../DSEmptyState/DSEmptyState.jsx";
import {DSImage} from "../DSImage/DSImage.jsx";
import {DSLinkMenu} from "../DSLinkMenu/DSLinkMenu";
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

  renderHero() {
    let [heroRec, ...otherRecs] = this.props.data.recommendations.slice(0, this.props.items);
    this.heroRec = heroRec;

    const cards = [];
    for (let index = 0; index < this.props.items - 1; index++) {
      const rec = otherRecs[index];
      cards.push(!rec || rec.placeholder ? (
        <PlaceholderDSCard key={`dscard-${index}`} />
      ) : (
        <DSCard
        campaignId={rec.campaign_id}
        key={`dscard-${index}`}
        image_src={rec.image_src}
        raw_image_src={rec.raw_image_src}
        title={rec.title}
        url={rec.url}
        id={rec.id}
        pos={rec.pos}
        type={this.props.type}
        dispatch={this.props.dispatch}
        context={rec.context}
        source={rec.domain}
        pocket_id={rec.pocket_id}
        bookmarkGuid={rec.bookmarkGuid} />
      ));
    }

    let heroCard = null;

    if (!heroRec || heroRec.placeholder) {
      heroCard = (
        <PlaceholderDSCard />
      );
    } else {
      heroCard = (
        <div className="ds-hero-item">
          <SafeAnchor
            className="wrapper"
            dispatch={this.props.dispatch}
            onLinkClick={this.onLinkClick}
            url={heroRec.url}>
            <div className="img-wrapper">
              <DSImage extraClassNames="img" source={heroRec.image_src} rawSource={heroRec.raw_image_src} />
            </div>
            <div className="meta">
              <div className="header-and-excerpt">
                {heroRec.context ? (
                  <p className="context">{heroRec.context}</p>
                ) : (
                  <p className="source clamp">{heroRec.domain}</p>
                )}
                <header className="clamp">{heroRec.title}</header>
                <p className="excerpt clamp">{heroRec.excerpt}</p>
              </div>
            </div>
            <ImpressionStats
              campaignId={heroRec.campaignId}
              rows={[{id: heroRec.id, pos: heroRec.pos}]}
              dispatch={this.props.dispatch}
              source={this.props.type} />
          </SafeAnchor>
          <DSLinkMenu
            id={heroRec.id}
            index={heroRec.pos}
            dispatch={this.props.dispatch}
            intl={this.props.intl}
            url={heroRec.url}
            title={heroRec.title}
            source={heroRec.domain}
            type={this.props.type}
            pocket_id={heroRec.pocket_id}
            bookmarkGuid={heroRec.bookmarkGuid} />
        </div>
      );
    }

    let list = (
      <List
        recStartingPoint={1}
        data={this.props.data}
        hasImages={true}
        hasBorders={this.props.border === `border`}
        items={this.props.items - 1}
        type={`Hero`} />
    );

    return (
      <div className={`ds-hero ds-hero-${this.props.border}`}>
        {heroCard}
        <div className={`${this.props.subComponentType}`}>
          { this.props.subComponentType === `cards` ? cards : list }
        </div>
      </div>
    );
  }

  render() {
    const {data} = this.props;

    // Handle a render before feed has been fetched by displaying nothing
    if (!data || !data.recommendations) {
      return (
        <div />
      );
    }

    // Handle the case where a user has dismissed all recommendations
    const isEmpty = data.recommendations.length === 0;

    return (
      <div>
        <div className="ds-header">{this.props.title}</div>
        {isEmpty ?
          <div className="ds-hero empty"><DSEmptyState /></div> :
          this.renderHero()
        }
      </div>
    );
  }
}

Hero.defaultProps = {
  data: {},
  border: `border`,
  items: 1, // Number of stories to display
};
