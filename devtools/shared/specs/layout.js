/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, generateActorSpec} = require("devtools/shared/protocol");

const reflowSpec = generateActorSpec({
  typeName: "reflow",

  events: {
    /**
     * The reflows event is emitted when reflows have been detected. The event
     * is sent with an array of reflows that occured. Each item has the
     * following properties:
     * - start {Number}
     * - end {Number}
     * - isInterruptible {Boolean}
     */
    reflows: {
      type: "reflows",
      reflows: Arg(0, "array:json")
    }
  },

  methods: {
    start: {oneway: true},
    stop: {oneway: true},
  },
});

exports.reflowSpec = reflowSpec;
