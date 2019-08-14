import { actionCreators as ac } from "common/Actions.jsm";
import { DSImage } from "../DSImage/DSImage.jsx";
import { DSLinkMenu } from "../DSLinkMenu/DSLinkMenu";
import { ImpressionStats } from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";

export class DSCard extends React.PureComponent {
  constructor(props) {
    super(props);

    this.onLinkClick = this.onLinkClick.bind(this);
    this.setPlaceholderRef = element => {
      this.placholderElement = element;
    };

    this.state = {
      isSeen: false,
    };
  }

  onLinkClick(event) {
    if (this.props.dispatch) {
      this.props.dispatch(
        ac.UserEvent({
          event: "CLICK",
          source: this.props.type.toUpperCase(),
          action_position: this.props.pos,
        })
      );

      this.props.dispatch(
        ac.ImpressionStats({
          source: this.props.type.toUpperCase(),
          click: 0,
          tiles: [
            {
              id: this.props.id,
              pos: this.props.pos,
              ...(this.props.shim && this.props.shim.click
                ? { shim: this.props.shim.click }
                : {}),
            },
          ],
        })
      );
    }
  }

  onSeen(entries) {
    if (this.state) {
      const entry = entries.find(e => e.isIntersecting);

      if (entry) {
        if (this.placholderElement) {
          this.observer.unobserve(this.placholderElement);
        }

        // Stop observing since element has been seen
        this.setState({
          isSeen: true,
        });
      }
    }
  }

  componentDidMount() {
    if (this.placholderElement) {
      this.observer = new IntersectionObserver(this.onSeen.bind(this));
      this.observer.observe(this.placholderElement);
    }
  }

  componentWillUnmount() {
    // Remove observer on unmount
    if (this.observer && this.placholderElement) {
      this.observer.unobserve(this.placholderElement);
    }
  }

  render() {
    if (this.props.placeholder || !this.state.isSeen) {
      return (
        <div className="ds-card placeholder" ref={this.setPlaceholderRef} />
      );
    }
    return (
      <div className="ds-card">
        <SafeAnchor
          className="ds-card-link"
          dispatch={this.props.dispatch}
          onLinkClick={!this.props.placeholder ? this.onLinkClick : undefined}
          url={this.props.url}
        >
          <div className="img-wrapper">
            <DSImage
              extraClassNames="img"
              source={this.props.image_src}
              rawSource={this.props.raw_image_src}
            />
          </div>
          <div className="meta">
            <div className="info-wrap">
              <p className="source clamp">{this.props.source}</p>
              <header className="title clamp">{this.props.title}</header>
              {this.props.excerpt && (
                <p className="excerpt clamp">{this.props.excerpt}</p>
              )}
            </div>
            {this.props.context && (
              <p className="context">{this.props.context}</p>
            )}
          </div>
          <ImpressionStats
            campaignId={this.props.campaignId}
            rows={[
              {
                id: this.props.id,
                pos: this.props.pos,
                ...(this.props.shim && this.props.shim.impression
                  ? { shim: this.props.shim.impression }
                  : {}),
              },
            ]}
            dispatch={this.props.dispatch}
            source={this.props.type}
          />
        </SafeAnchor>
        <DSLinkMenu
          id={this.props.id}
          index={this.props.pos}
          dispatch={this.props.dispatch}
          url={this.props.url}
          title={this.props.title}
          source={this.props.source}
          type={this.props.type}
          pocket_id={this.props.pocket_id}
          shim={this.props.shim}
          bookmarkGuid={this.props.bookmarkGuid}
        />
      </div>
    );
  }
}
export const PlaceholderDSCard = props => <DSCard placeholder={true} />;
