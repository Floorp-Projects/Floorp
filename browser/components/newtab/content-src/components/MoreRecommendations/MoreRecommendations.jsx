import {FormattedMessage} from "react-intl";
import React from "react";

export class MoreRecommendations extends React.PureComponent {
  render() {
    const {read_more_endpoint} = this.props;
    if (read_more_endpoint) {
      return (
        <a className="more-recommendations" href={read_more_endpoint}>
          <FormattedMessage id="pocket_more_reccommendations" />
        </a>
      );
    }
    return null;
  }
}
