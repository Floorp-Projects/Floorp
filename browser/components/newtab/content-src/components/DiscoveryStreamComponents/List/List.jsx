import {connect} from "react-redux";
import React from "react";

export class _List extends React.PureComponent {
  render() {
    // const feed = this.props.DiscoveryStream.feeds[this.props.feed.url];
    return (
      <div className="ds-list">
        List
      </div>
    );
  }
}

export const List = connect(state => ({DiscoveryStream: state.DiscoveryStream}))(_List);
