/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { CardGrid } from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import React from "react";
import { connect } from "react-redux";

export class _CollectionCardGrid extends React.PureComponent {
  render() {
    const { placement, DiscoveryStream, data, feed } = this.props;

    // Handle a render before feed has been fetched by displaying nothing
    if (!data) {
      return null;
    }

    const { title, context } = DiscoveryStream.spocs.data[placement.name] || {};

    // Generally a card grid displays recs with spocs already injected.
    // Normally it doesn't care which rec is a spoc and which isn't,
    // it just displays content in a grid.
    // For collections, we're only displaying a list of spocs.
    // We don't need to tell the card grid that our list of cards are spocs,
    // it shouldn't need to care. So we just pass our spocs along as recs.
    // Think of it as injecting all rec positions with spocs.
    // Consider maybe making recommendations in CardGrid use a more generic name.
    const recsData = {
      recommendations: data.spocs,
    };
    return (
      <div className="ds-collection-card-grid">
        <CardGrid
          title={title}
          context={context}
          data={recsData}
          feed={feed}
          border={this.props.border}
          type={this.props.type}
          dispatch={this.props.dispatch}
          items={this.props.items}
        />
      </div>
    );
  }
}

export const CollectionCardGrid = connect(state => ({
  DiscoveryStream: state.DiscoveryStream,
}))(_CollectionCardGrid);
