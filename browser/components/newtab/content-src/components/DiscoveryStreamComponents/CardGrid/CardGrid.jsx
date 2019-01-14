import {connect} from "react-redux";
import React from "react";

export class _CardGrid extends React.PureComponent {
  render() {
    // const feed = this.props.DiscoveryStream.feeds[this.props.feed.url];
    return (
      <div>
        Card Grid
      </div>
    );
  }
}

export const CardGrid = connect(state => ({DiscoveryStream: state.DiscoveryStream}))(_CardGrid);
