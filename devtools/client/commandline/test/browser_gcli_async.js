/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// PLEASE TALK TO SOMEONE IN DEVELOPER TOOLS BEFORE EDITING IT

const exports = {};

function test() {
  helpers.runTestModule(exports, "browser_gcli_async.js");
}

// var helpers = require('./helpers');

exports.testBasic = function (options) {
  return helpers.audit(options, [
    {
      setup:    "tsslo",
      check: {
        input:  "tsslo",
        hints:       "w",
        markup: "IIIII",
        cursor: 5,
        current: "__command",
        status: "ERROR",
        predictions: ["tsslow"],
        unassigned: [ ]
      }
    },
    {
      setup:    "tsslo<TAB>",
      check: {
        input:  "tsslow ",
        hints:         "Shalom",
        markup: "VVVVVVV",
        cursor: 7,
        current: "hello",
        status: "ERROR",
        predictions: [
          "Shalom", "Namasté", "Hallo", "Dydd-da", "Chào", "Hej",
          "Saluton", "Sawubona"
        ],
        unassigned: [ ],
        args: {
          command: { name: "tsslow" },
          hello: {
            arg: "",
            status: "INCOMPLETE"
          },
        }
      }
    },
    {
      setup:    "tsslow S",
      check: {
        input:  "tsslow S",
        hints:          "halom",
        markup: "VVVVVVVI",
        cursor: 8,
        current: "hello",
        status: "ERROR",
        predictions: [ "Shalom", "Saluton", "Sawubona", "Namasté" ],
        unassigned: [ ],
        args: {
          command: { name: "tsslow" },
          hello: {
            arg: " S",
            status: "INCOMPLETE"
          },
        }
      }
    },
    {
      setup:    "tsslow S<TAB>",
      check: {
        input:  "tsslow Shalom ",
        hints:                "",
        markup: "VVVVVVVVVVVVVV",
        cursor: 14,
        current: "hello",
        status: "VALID",
        predictions: [ "Shalom" ],
        unassigned: [ ],
        args: {
          command: { name: "tsslow" },
          hello: {
            arg: " Shalom ",
            status: "VALID",
            message: ""
          },
        }
      }
    }
  ]);
};
