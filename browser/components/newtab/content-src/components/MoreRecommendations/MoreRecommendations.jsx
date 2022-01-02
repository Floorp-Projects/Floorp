/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

export class MoreRecommendations extends React.PureComponent {
  render() {
    const { read_more_endpoint } = this.props;
    if (read_more_endpoint) {
      return (
        <a
          className="more-recommendations"
          href={read_more_endpoint}
          data-l10n-id="newtab-pocket-more-recommendations"
        />
      );
    }
    return null;
  }
}
