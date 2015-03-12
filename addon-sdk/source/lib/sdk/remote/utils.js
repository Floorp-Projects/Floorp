/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Class } = require('../core/heritage');
const { List, addListItem, removeListItem } = require('../util/list');
const { emit } = require('../event/core');
const { pipe } = require('../event/utils');

// A helper class that maintains a list of EventTargets. Any events emitted
// to an EventTarget are also emitted by the EventParent. Likewise for an
// EventTarget's port property.
const EventParent = Class({
  implements: [ List ],

  attachItem: function(item) {
    addListItem(this, item);

    pipe(item.port, this.port);
    pipe(item, this);

    item.once('detach', () => {
      removeListItem(this, item);
    })

    emit(this, 'attach', item);
  },

  // Calls listener for every object already in the list and every object
  // subsequently added to the list.
  forEvery: function(listener) {
    for (let item of this)
      listener(item);

    this.on('attach', listener);
  }
});
exports.EventParent = EventParent;
