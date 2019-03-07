import {DSCard} from "../DSCard/DSCard.jsx";
import React from "react";

export class CardGrid extends React.PureComponent {
  render() {
    const {data} = this.props;

    // Handle a render before feed has been fetched by displaying nothing
    if (!data) {
      return (
        <div />
      );
    }

    let cards = data.recommendations.slice(0, this.props.items).map((rec, index) => (
      <DSCard
        key={`dscard-${index}`}
        pos={rec.pos}
        campaignId={rec.campaign_id}
        image_src={rec.image_src}
        title={rec.title}
        excerpt={rec.excerpt}
        url={rec.url}
        id={rec.id}
        type={this.props.type}
        context={rec.context}
        dispatch={this.props.dispatch}
        source={rec.domain} />
    ));

    let divisibility = ``;

    if (this.props.items % 4 === 0) {
      divisibility = `divisible-by-4`;
    } else if (this.props.items % 3 === 0) {
      divisibility = `divisible-by-3`;
    }

    return (
      <div>
        <div className="ds-header">{this.props.title}</div>
        <div className={`ds-card-grid ds-card-grid-${this.props.border} ds-card-grid-${divisibility}`}>
          {cards}
        </div>
      </div>
    );
  }
}

CardGrid.defaultProps = {
  border: `border`,
  items: 4, // Number of stories to display
};
