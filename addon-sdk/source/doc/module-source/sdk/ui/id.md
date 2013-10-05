<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `ui/id` module provides the `identify` helper method for creating
and defining UI component IDs.

<api name="identify">
@function
Makes and/or gets a unique ID for the input.

Making an ID

    const { identify } = require('sdk/ui/id');

    const Thingy = Class({
      initialize: function(details) {
        let id = identify(this);
      }
    });

Getting an ID

    const { identify } = require('sdk/ui/id');
    const { Thingy } = require('./thingy');

    let thing = Thingy(/* ... */);
    let thingID = identify(thing);

Defining ID generator

    const { identify } = require('sdk/ui/id');

    const Thingy = Class(/* ... */);

    identify.define(Thingy, thing => thing.guid);

@param object {Object}
  Object to create an ID for.
@returns {String}
  Returns a UUID by default (or domain specific ID based on a provided definition).
</api>
