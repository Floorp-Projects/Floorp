import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import React from "react";

const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
const INTERSECTION_RATIO = 0.5;

export class DSCard extends React.PureComponent {
  constructor(props) {
    super(props);

    this.cardElementRef = this.cardElementRef.bind(this);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  componentDidMount() {
    if (this.props.document.visibilityState === VISIBLE) {
      this.setupIntersectionObserver();
    } else {
      this._onVisibilityChange = () => {
        if (this.props.document.visibilityState === VISIBLE) {
          this.setupIntersectionObserver();
          this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
        }
      };
      this.props.document.addEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
  }

  componentWillUnmount() {
    if (this._onVisibilityChange) {
      this.props.document.removeEventListener(VISIBILITY_CHANGE_EVENT, this._onVisibilityChange);
    }
    if (this._intersectionObserver) {
      this._intersectionObserver.unobserve(this.cardElement);
    }
  }

  setupIntersectionObserver() {
    const options = {threshold: INTERSECTION_RATIO};
    this._intersectionObserver = new IntersectionObserver(entries => {
      for (let entry of entries) {
        if (entry.isIntersecting && entry.intersectionRatio >= INTERSECTION_RATIO) {
          this.dispatchSpocImpression();
          break;
        }
      }
    }, options);
    this._intersectionObserver.observe(this.cardElement);
  }

  dispatchSpocImpression() {
    if (this.props.campaignId) {
      this.props.dispatch(ac.OnlyToMain({type: at.DISCOVERY_STREAM_SPOC_IMPRESSION, data: {campaignId: this.props.campaignId}}));
    }
    this._intersectionObserver.unobserve(this.cardElement);
  }

  cardElementRef(element) {
    this.cardElement = element;
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
      <a href={this.props.url} className="ds-card" onClick={this.onLinkClick} ref={this.cardElementRef}>
        <div className="img-wrapper">
          <div className="img" style={{backgroundImage: `url(${this.props.image_src}`}} />
        </div>
        <div className="meta">
          <div className="info-wrap">
            <header className="title">{this.props.title}</header>
            {this.props.excerpt && <p className="excerpt">{this.props.excerpt}</p>}
          </div>
          <p>
            {this.props.context && (
              <span>
                <span className="context">{this.props.context}</span>
                <br />
              </span>
            )}
            <span className="source">{this.props.source}</span>
          </p>
        </div>
      </a>
    );
  }
}

DSCard.defaultProps = {
  document: global.document,
};
