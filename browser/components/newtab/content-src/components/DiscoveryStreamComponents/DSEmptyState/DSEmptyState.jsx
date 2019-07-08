import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import React from "react";

export class DSEmptyState extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onReset = this.onReset.bind(this);
    this.state = {};
  }

  componentWillUnmount() {
    if (this.timeout) {
      clearTimeout(this.timeout);
    }
  }

  onReset() {
    if (this.props.dispatch && this.props.feed) {
      const { feed } = this.props;
      const { url } = feed;
      this.props.dispatch({
        type: at.DISCOVERY_STREAM_FEED_UPDATE,
        data: {
          feed: {
            ...feed,
            data: {
              ...feed.data,
              status: "waiting",
            },
          },
          url,
        },
      });

      this.setState({ waiting: true });
      this.timeout = setTimeout(() => {
        this.timeout = null;
        this.setState({
          waiting: false,
        });
      }, 300);

      this.props.dispatch(
        ac.OnlyToMain({ type: at.DISCOVERY_STREAM_RETRY_FEED, data: { feed } })
      );
    }
  }

  renderButton() {
    if (this.props.status === "waiting" || this.state.waiting) {
      return <button className="try-again-button waiting">Loading...</button>;
    }

    return (
      <button className="try-again-button" onClick={this.onReset}>
        Try Again
      </button>
    );
  }

  renderState() {
    if (this.props.status === "waiting" || this.props.status === "failed") {
      return (
        <React.Fragment>
          <h2>Oops! We almost loaded this section, but not quite.</h2>
          {this.renderButton()}
        </React.Fragment>
      );
    }

    return (
      <React.Fragment>
        <h2>You are caught up!</h2>
        <p>Check back later for more stories.</p>
      </React.Fragment>
    );
  }

  render() {
    return (
      <div className="section-empty-state">
        <div className="empty-state-message">{this.renderState()}</div>
      </div>
    );
  }
}
