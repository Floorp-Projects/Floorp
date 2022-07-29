/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import { CardGrid } from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import { DSDismiss } from "content-src/components/DiscoveryStreamComponents/DSDismiss/DSDismiss";
import { LinkMenuOptions } from "content-src/lib/link-menu-options";
import React from "react";

export class CollectionCardGrid extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onDismissClick = this.onDismissClick.bind(this);
    this.state = {
      dismissed: false,
    };
  }

  onDismissClick() {
    const { data } = this.props;
    if (this.props.dispatch && data && data.spocs && data.spocs.length) {
      this.setState({
        dismissed: true,
      });
      const pos = 0;
      const source = this.props.type.toUpperCase();
      // Grab the available items in the array to dismiss.
      // This fires a ping for all items available, even if below the fold.
      const spocsData = data.spocs.map(item => ({
        url: item.url,
        guid: item.id,
        shim: item.shim,
        flight_id: item.flightId,
      }));

      const blockUrlOption = LinkMenuOptions.BlockUrls(spocsData, pos, source);
      const { action, impression, userEvent } = blockUrlOption;
      this.props.dispatch(action);

      this.props.dispatch(
        ac.DiscoveryStreamUserEvent({
          event: userEvent,
          source,
          action_position: pos,
        })
      );
      if (impression) {
        this.props.dispatch(impression);
      }
    }
  }

  render() {
    const { data, dismissible, pocket_button_enabled } = this.props;
    if (
      this.state.dismissed ||
      !data ||
      !data.spocs ||
      !data.spocs[0] ||
      // We only display complete collections.
      data.spocs.length < 3
    ) {
      return null;
    }
    const { spocs, placement, feed } = this.props;
    // spocs.data is spocs state data, and not an array of spocs.
    const { title, context, sponsored_by_override, sponsor } =
      spocs.data[placement.name] || {};
    // Just in case of bad data, don't display a broken collection.
    if (!title) {
      return null;
    }

    let sponsoredByMessage = "";

    // If override is not false or an empty string.
    if (sponsored_by_override || sponsored_by_override === "") {
      // We specifically want to display nothing if the server returns an empty string.
      // So the server can turn off the label.
      // This is to support the use cases where the sponsored context is displayed elsewhere.
      sponsoredByMessage = sponsored_by_override;
    } else if (sponsor) {
      sponsoredByMessage = {
        id: `newtab-label-sponsored-by`,
        values: { sponsor },
      };
    } else if (context) {
      sponsoredByMessage = context;
    }

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

    // All cards inside of a collection card grid have a slightly different type.
    // For the case of interactions to the card grid, we use the type "COLLECTIONCARDGRID".
    // Example, you dismiss the whole collection, we use the type "COLLECTIONCARDGRID".
    // For interactions inside the card grid, example, you dismiss a single card in the collection,
    // we use the type "COLLECTIONCARDGRID_CARD".
    const type = `${this.props.type}_card`;

    const collectionGrid = (
      <div className="ds-collection-card-grid">
        <CardGrid
          pocket_button_enabled={pocket_button_enabled}
          title={title}
          context={sponsoredByMessage}
          data={recsData}
          feed={feed}
          type={type}
          is_collection={true}
          dispatch={this.props.dispatch}
          items={this.props.items}
        />
      </div>
    );

    if (dismissible) {
      return (
        <DSDismiss
          onDismissClick={this.onDismissClick}
          extraClasses={`ds-dismiss-ds-collection`}
        >
          {collectionGrid}
        </DSDismiss>
      );
    }
    return collectionGrid;
  }
}
