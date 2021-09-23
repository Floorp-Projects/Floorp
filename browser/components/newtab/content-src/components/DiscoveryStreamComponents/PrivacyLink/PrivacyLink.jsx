/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText";

export class PrivacyLink extends React.PureComponent {
  render() {
    const { properties } = this.props;
    return (
      <div className="ds-privacy-link">
        <SafeAnchor url={properties.url}>
          <FluentOrText message={properties.title} />
        </SafeAnchor>
      </div>
    );
  }
}
