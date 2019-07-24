/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  workersReducer,
} = require("devtools/client/application/src/reducers/workers-state");
const {
  pageReducer,
} = require("devtools/client/application/src/reducers/page-state");

exports.reducers = {
  workers: workersReducer,
  page: pageReducer,
};
