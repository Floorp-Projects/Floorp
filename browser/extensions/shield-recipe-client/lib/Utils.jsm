/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://shield-recipe-client/lib/LogManager.jsm");

this.EXPORTED_SYMBOLS = ["Utils"];

const log = LogManager.getLogger("utils");

this.Utils = {
  /**
   * Convert an array of objects to an object. Each item is keyed by the value
   * of the given key on the item.
   *
   * > list = [{foo: "bar"}, {foo: "baz"}]
   * > keyBy(list, "foo") == {bar: {foo: "bar"}, baz: {foo: "baz"}}
   *
   * @param  {Array} list
   * @param  {String} key
   * @return {Object}
   */
  keyBy(list, key) {
    return list.reduce((map, item) => {
      if (!(key in item)) {
        log.warn(`Skipping list due to missing key "${key}".`);
        return map;
      }

      map[item[key]] = item;
      return map;
    }, {});
  },
};
