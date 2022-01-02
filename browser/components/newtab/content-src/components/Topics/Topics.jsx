/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

export class Topic extends React.PureComponent {
  render() {
    const { url, name } = this.props;
    return (
      <li>
        <a key={name} href={url}>
          {name}
        </a>
      </li>
    );
  }
}

export class Topics extends React.PureComponent {
  render() {
    const { topics } = this.props;
    return (
      <span className="topics">
        <span data-l10n-id="newtab-pocket-read-more" />
        <ul>
          {topics &&
            topics.map(t => <Topic key={t.name} url={t.url} name={t.name} />)}
        </ul>
      </span>
    );
  }
}
