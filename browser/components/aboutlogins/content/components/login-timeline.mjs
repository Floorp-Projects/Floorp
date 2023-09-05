/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  styleMap,
  classMap,
  html,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export default class Timeline extends MozLitElement {
  static get properties() {
    return {
      history: { type: Array },
    };
  }

  constructor() {
    super();
    this.history = [];
  }

  render() {
    this.history = this.history.filter(historyPoint => historyPoint.time);
    this.history.sort((a, b) => a.time - b.time);
    let columns = "auto";

    // Add each history event to the timeline
    let points = this.history.map((entry, index) => {
      if (index > 0) {
        // add a gap between previous point and current one
        columns += ` ${entry.time - this.history[index - 1].time}fr auto`;
      }

      let columnNumber = 2 * index + 1;
      let styles = styleMap({ gridColumn: columnNumber });
      return html`
        <svg
          style=${styles}
          class="point"
          viewBox="0 0 300 150"
          xmlns="http://www.w3.org/2000/svg"
        >
          <circle cx="150" cy="75" r="75" />
        </svg>
        <div
          style=${styles}
          class="date"
          data-l10n-id="login-item-timeline-point-date"
          data-l10n-args=${JSON.stringify({ datetime: entry.time })}
        ></div>
        <div
          style=${styles}
          class="action"
          data-l10n-id=${entry.actionId}
        </div>
      `;
    });

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/aboutlogins/components/login-timeline.css"
      />
      <div
        class="timeline ${classMap({ empty: !this.history.length })}"
        style=${styleMap({ gridTemplateColumns: columns })}
      >
        <div class="line"></div>
        <div class="line"></div>
        <div class="line"></div>
        ${points}
      </div>
    `;
  }
}

customElements.define("login-timeline", Timeline);
