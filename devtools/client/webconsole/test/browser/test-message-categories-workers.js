/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global fooBarWorker*/
/* eslint-disable no-unused-vars*/

"use strict";

var onmessage = function() {
  fooBarWorker();
};
