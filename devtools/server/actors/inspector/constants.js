/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Any event listener flagged with this symbol will not be considered when
 * the EventCollector class enumerates listeners for nodes. For example:
 *
 *   const someListener = () => {};
 *   someListener[EXCLUDED_LISTENER] = true;
 *   node.addEventListener("event", someListener);
 */
const EXCLUDED_LISTENER = Symbol("event-collector-excluded-listener");

exports.EXCLUDED_LISTENER = EXCLUDED_LISTENER;
