/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env module */

const { Interactions } = ChromeUtils.import(
  "resource:///modules/Interactions.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PlacesUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesUtils.jsm"
);

const metadataHandler = new (class {
  /**
   * Maximum number of rows to display by default.
   *
   * @typedef {number}
   */
  #maxRows = 100;

  /**
   * A reference to the database connection.
   *
   * @typedef {mozIStorageConnection}
   */
  #db = null;

  /**
   * A map of columns that are displayed by default.
   * @note If you change the number of columns, then you also need to change
   *       the css to account for the new number.
   *
   * - The key is the column name in the database.
   * - The header is the column header on the table.
   * - The modifier is a function to modify the returned value from the database
   *   for display.
   * - includeTitle determines if the title attribute should be set on that
   *   column, for tooltips, e.g. if an element is likely to overflow.
   *
   * @typedef {Map<string, object>}
   */
  #columnMap = new Map([
    ["id", { header: "ID" }],
    ["url", { header: "URL", includeTitle: true }],
    [
      "updated_at",
      {
        header: "Updated",
        modifier: updatedAt => new Date(updatedAt).toLocaleString(),
      },
    ],
    [
      "total_view_time",
      {
        header: "View Time (s)",
        modifier: totalViewTime => (totalViewTime / 1000).toFixed(2),
      },
    ],
    [
      "typing_time",
      {
        header: "Typing Time (s)",
        modifier: typingTime => (typingTime / 1000).toFixed(2),
      },
    ],
    ["key_presses", { header: "Key Presses" }],
  ]);

  async start() {
    this.#setupUI();
    this.#db = await PlacesUtils.promiseDBConnection();
    await this.#updateDisplay();
    setInterval(this.#updateDisplay.bind(this), 10000);
  }

  /**
   * Creates the initial table layout to the correct size.
   */
  #setupUI() {
    let tableBody = document.createDocumentFragment();
    let header = document.createDocumentFragment();
    for (let details of this.#columnMap.values()) {
      let columnDiv = document.createElement("div");
      columnDiv.textContent = details.header;
      header.appendChild(columnDiv);
    }
    tableBody.appendChild(header);

    for (let i = 0; i < this.#maxRows; i++) {
      let row = document.createDocumentFragment();
      for (let j = 0; j < this.#columnMap.size; j++) {
        row.appendChild(document.createElement("div"));
      }
      tableBody.appendChild(row);
    }
    let viewer = document.getElementById("metadataViewer");
    viewer.appendChild(tableBody);

    let limit = document.getElementById("metadataLimit");
    limit.textContent = `Maximum rows displayed: ${this.#maxRows}.`;
  }

  /**
   * Loads the current metadata from the database and updates the display.
   */
  async #updateDisplay() {
    let rows = await this.#db.executeCached(
      `SELECT m.id AS id, h.url AS url, updated_at, total_view_time,
              typing_time, key_presses FROM moz_places_metadata m
       JOIN moz_places h ON h.id = m.place_id
       ORDER BY updated_at DESC
       LIMIT ${this.#maxRows}`
    );
    let viewer = document.getElementById("metadataViewer");
    let index = this.#columnMap.size;
    for (let row of rows) {
      for (let [column, details] of this.#columnMap.entries()) {
        let value = row.getResultByName(column);

        if (details.includeTitle) {
          viewer.children[index].setAttribute("title", value);
        }

        viewer.children[index].textContent = details.modifier
          ? details.modifier(value)
          : value;

        index++;
      }
    }
  }
})();

function checkPrefs() {
  if (
    !Services.prefs.getBoolPref("browser.places.interactions.enabled", false)
  ) {
    let warning = document.getElementById("enabledWarning");
    warning.hidden = false;
  }
}

checkPrefs();
metadataHandler.start().catch(console.error);
