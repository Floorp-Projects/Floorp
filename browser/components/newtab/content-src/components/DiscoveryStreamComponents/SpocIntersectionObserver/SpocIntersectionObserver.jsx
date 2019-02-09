import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import React from "react";
const VISIBLE = "visible";
const VISIBILITY_CHANGE_EVENT = "visibilitychange";
const INTERSECTION_RATIO = 0.5;

export class SpocIntersectionObserver extends React.PureComponent {
  constructor(props) {
    super(props);

    this.spocElementRef = this.spocElementRef.bind(this);
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
      this._intersectionObserver.unobserve(this.spocElement);
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
    this._intersectionObserver.observe(this.spocElement);
  }

  dispatchSpocImpression() {
    if (this.props.campaignId) {
      this.props.dispatch(ac.OnlyToMain({type: at.DISCOVERY_STREAM_SPOC_IMPRESSION, data: {campaignId: this.props.campaignId}}));
    }
    this._intersectionObserver.unobserve(this.spocElement);
  }

  spocElementRef(element) {
    this.spocElement = element;
  }

  render() {
    return (
      <div ref={this.spocElementRef}>
        {this.props.children}
      </div>
    );
  }
}

SpocIntersectionObserver.defaultProps = {
  document: global.document,
};
