/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PageDataCollector"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

/**
 * Each `PageDataCollector` is responsible for finding data about a DOM
 * document. When initialized it must asynchronously discover available data and
 * either report what was found or an empty array if there was no relevant data
 * in the page. Following this it may continue to monitor the page and report as
 * the available data changes.
 */
class PageDataCollector extends EventEmitter {
  /**
   * Supported data types.
   */
  static get DATA_TYPE() {
    return {
      PRODUCT: 1,
      GENERAL: 2,
    };
  }

  /**
   * Internal, should generally not need to be overriden by child classes.
   *
   * @param {Document} document
   *   The DOM Document for the page.
   */
  constructor(document) {
    super();
    this.document = document;
  }

  /**
   * Starts collection of data, should be overriden by child classes. The
   * current state of data in the page should be asynchronously returned from
   * this method.
   *
   * @returns {Data[]} The data found for the page which may be an empty array.
   */
  async init() {
    return [];
  }

  /**
   * Signals that the page has been destroyed.
   */
  destroy() {}

  /**
   * Should not be overriden by child classes. Call to signal that the data in
   * the page changed.
   *
   * @param {Data[]} data
   *   The data found which may be an empty array to signal that no data was found.
   */
  dataFound(data) {
    this.emit("data", data);
  }
}
