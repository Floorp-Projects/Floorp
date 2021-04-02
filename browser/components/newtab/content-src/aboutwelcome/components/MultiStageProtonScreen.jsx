/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "./MSLocalized";

export class MultiStageProtonScreen extends React.PureComponent {
  render() {
    const { content } = this.props;

    return (
      <main className={`screen ${this.props.id}`}>
        <div className="welcome-text">
          <Localized text={content.title}>
            <h1 />
          </Localized>
          <Localized text={content.subtitle}>
            <h2 />
          </Localized>
        </div>
        <div>
          <Localized
            text={content.primary_button ? content.primary_button.label : null}
          >
            <button
              className="primary"
              value="primary_button"
              onClick={this.props.handleAction}
            />
          </Localized>
        </div>
      </main>
    );
  }
}
