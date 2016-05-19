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
  helpers.runTestModule(exports, "browser_gcli_cli2.js");
}

// var helpers = require('./helpers');

exports.testSingleString = function (options) {
  return helpers.audit(options, [
    {
      setup:    "tsr",
      check: {
        input:  "tsr",
        hints:     " <text>",
        markup: "VVV",
        cursor: 3,
        current: "__command",
        status: "ERROR",
        unassigned: [ ],
        args: {
          command: { name: "tsr" },
          text: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018text\u2019."
          }
        }
      }
    },
    {
      setup:    "tsr ",
      check: {
        input:  "tsr ",
        hints:      "<text>",
        markup: "VVVV",
        cursor: 4,
        current: "text",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsr" },
          text: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018text\u2019."
          }
        }
      }
    },
    {
      setup:    "tsr h",
      check: {
        input:  "tsr h",
        hints:       "",
        markup: "VVVVV",
        cursor: 5,
        current: "text",
        status: "VALID",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsr" },
          text: {
            value: "h",
            arg: " h",
            status: "VALID",
            message: ""
          }
        }
      }
    },
    {
      setup:    'tsr "h h"',
      check: {
        input:  'tsr "h h"',
        hints:           "",
        markup: "VVVVVVVVV",
        cursor: 9,
        current: "text",
        status: "VALID",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsr" },
          text: {
            value: "h h",
            arg: ' "h h"',
            status: "VALID",
            message: ""
          }
        }
      }
    },
    {
      setup:    "tsr h h h",
      check: {
        input:  "tsr h h h",
        hints:           "",
        markup: "VVVVVVVVV",
        cursor: 9,
        current: "text",
        status: "VALID",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsr" },
          text: {
            value: "h h h",
            arg: " h h h",
            status: "VALID",
            message: ""
          }
        }
      }
    }
  ]);
};

exports.testSingleNumber = function (options) {
  return helpers.audit(options, [
    {
      setup:    "tsu",
      check: {
        input:  "tsu",
        hints:     " <num>",
        markup: "VVV",
        cursor: 3,
        current: "__command",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsu" },
          num: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018num\u2019."
          }
        }
      }
    },
    {
      setup:    "tsu ",
      check: {
        input:  "tsu ",
        hints:      "<num>",
        markup: "VVVV",
        cursor: 4,
        current: "num",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsu" },
          num: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018num\u2019."
          }
        }
      }
    },
    {
      setup:    "tsu 1",
      check: {
        input:  "tsu 1",
        hints:       "",
        markup: "VVVVV",
        cursor: 5,
        current: "num",
        status: "VALID",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsu" },
          num: { value: 1, arg: " 1", status: "VALID", message: "" }
        }
      }
    },
    {
      setup:    "tsu x",
      check: {
        input:  "tsu x",
        hints:       "",
        markup: "VVVVE",
        cursor: 5,
        current: "num",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        tooltipState: "true:isError",
        args: {
          command: { name: "tsu" },
          num: {
            value: undefined,
            arg: " x",
            status: "ERROR",
            message: "Can\u2019t convert \u201cx\u201d to a number."
          }
        }
      }
    },
    {
      setup:    "tsu 1.5",
      check: {
        input:  "tsu 1.5",
        hints:       "",
        markup: "VVVVEEE",
        cursor: 7,
        current: "num",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsu" },
          num: {
            value: undefined,
            arg: " 1.5",
            status: "ERROR",
            message: "Can\u2019t convert \u201c1.5\u201d to an integer."
          }
        }
      }
    }
  ]);
};

exports.testSingleFloat = function (options) {
  return helpers.audit(options, [
    {
      setup:    "tsf",
      check: {
        input:  "tsf",
        hints:     " <num>",
        markup: "VVV",
        cursor: 3,
        current: "__command",
        status: "ERROR",
        error: "",
        unassigned: [ ],
        args: {
          command: { name: "tsf" },
          num: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018num\u2019."
          }
        }
      }
    },
    {
      setup:    "tsf 1",
      check: {
        input:  "tsf 1",
        hints:       "",
        markup: "VVVVV",
        cursor: 5,
        current: "num",
        status: "VALID",
        error: "",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsf" },
          num: { value: 1, arg: " 1", status: "VALID", message: "" }
        }
      }
    },
    {
      setup:    "tsf 1.",
      check: {
        input:  "tsf 1.",
        hints:        "",
        markup: "VVVVVV",
        cursor: 6,
        current: "num",
        status: "VALID",
        error: "",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsf" },
          num: { value: 1, arg: " 1.", status: "VALID", message: "" }
        }
      }
    },
    {
      setup:    "tsf 1.5",
      check: {
        input:  "tsf 1.5",
        hints:         "",
        markup: "VVVVVVV",
        cursor: 7,
        current: "num",
        status: "VALID",
        error: "",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsf" },
          num: { value: 1.5, arg: " 1.5", status: "VALID", message: "" }
        }
      }
    },
    {
      setup:    "tsf 1.5x",
      check: {
        input:  "tsf 1.5x",
        hints:          "",
        markup: "VVVVVVVV",
        cursor: 8,
        current: "num",
        status: "VALID",
        error: "",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsf" },
          num: { value: 1.5, arg: " 1.5x", status: "VALID", message: "" }
        }
      }
    },
    {
      name: "tsf x (cursor=4)",
      setup: function () {
        return helpers.setInput(options, "tsf x", 4);
      },
      check: {
        input:  "tsf x",
        hints:       "",
        markup: "VVVVE",
        cursor: 4,
        current: "num",
        status: "ERROR",
        error: "Can\u2019t convert \u201cx\u201d to a number.",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsf" },
          num: {
            value: undefined,
            arg: " x",
            status: "ERROR",
            message: "Can\u2019t convert \u201cx\u201d to a number."
          }
        }
      }
    }
  ]);
};

exports.testElementWeb = function (options) {
  return helpers.audit(options, [
    {
      setup:    "tse #gcli-root",
      check: {
        input:  "tse #gcli-root",
        hints:                 " [options]",
        markup: "VVVVVVVVVVVVVV",
        cursor: 14,
        current: "node",
        status: "VALID",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tse" },
          node: {
            arg: " #gcli-root",
            status: "VALID",
            message: ""
          },
          nodes: { arg: "", status: "VALID", message: "" },
          nodes2: { arg: "", status: "VALID", message: "" },
        }
      }
    }
  ]);
};

exports.testElement = function (options) {
  return helpers.audit(options, [
    {
      setup:    "tse",
      check: {
        input:  "tse",
        hints:     " <node> [options]",
        markup: "VVV",
        cursor: 3,
        current: "__command",
        status: "ERROR",
        predictions: [ "tse", "tselarr" ],
        unassigned: [ ],
        args: {
          command: { name: "tse" },
          node: { arg: "", status: "INCOMPLETE" },
          nodes: { arg: "", status: "VALID", message: "" },
          nodes2: { arg: "", status: "VALID", message: "" },
        }
      }
    },
    {
      setup:    "tse #gcli-nomatch",
      check: {
        input:  "tse #gcli-nomatch",
        hints:                   " [options]",
        markup: "VVVVIIIIIIIIIIIII",
        cursor: 17,
        current: "node",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        outputState: "false:default",
        tooltipState: "true:isError",
        args: {
          command: { name: "tse" },
          node: {
            value: undefined,
            arg: " #gcli-nomatch",
            // This is somewhat debatable because this input can't be corrected
            // simply by typing so it's and error rather than incomplete,
            // however without digging into the CSS engine we can't tell that
            // so we default to incomplete
            status: "INCOMPLETE",
            message: "No matches"
          },
          nodes: { arg: "", status: "VALID", message: "" },
          nodes2: { arg: "", status: "VALID", message: "" },
        }
      }
    },
    {
      setup:    "tse #",
      check: {
        input:  "tse #",
        hints:       " [options]",
        markup: "VVVVE",
        cursor: 5,
        current: "node",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        tooltipState: "true:isError",
        args: {
          command: { name: "tse" },
          node: {
            value: undefined,
            arg: " #",
            status: "ERROR",
            message: "Syntax error in CSS query"
          },
          nodes: { arg: "", status: "VALID", message: "" },
          nodes2: { arg: "", status: "VALID", message: "" },
        }
      }
    },
    {
      setup:    "tse .",
      check: {
        input:  "tse .",
        hints:       " [options]",
        markup: "VVVVE",
        cursor: 5,
        current: "node",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        tooltipState: "true:isError",
        args: {
          command: { name: "tse" },
          node: {
            value: undefined,
            arg: " .",
            status: "ERROR",
            message: "Syntax error in CSS query"
          },
          nodes: { arg: "", status: "VALID", message: "" },
          nodes2: { arg: "", status: "VALID", message: "" },
        }
      }
    },
    {
      setup:    "tse *",
      check: {
        input:  "tse *",
        hints:       " [options]",
        markup: "VVVVE",
        cursor: 5,
        current: "node",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        tooltipState: "true:isError",
        args: {
          command: { name: "tse" },
          node: {
            value: undefined,
            arg: " *",
            status: "ERROR",
            message: /^Too many matches \([0-9]*\)/
          },
          nodes: { arg: "", status: "VALID", message: "" },
          nodes2: { arg: "", status: "VALID", message: "" },
        }
      }
    }
  ]);
};

exports.testNestedCommand = function (options) {
  return helpers.audit(options, [
    {
      setup:    "tsn",
      check: {
        input:  "tsn",
        hints:     " deep down nested cmd",
        markup: "III",
        cursor: 3,
        current: "__command",
        status: "ERROR",
        predictionsInclude: [
          "tsn deep", "tsn deep down", "tsn deep down nested",
          "tsn deep down nested cmd", "tsn dif"
        ],
        unassigned: [ ],
        args: {
          command: { name: "tsn" }
        }
      }
    },
    {
      setup:    "tsn ",
      check: {
        input:  "tsn ",
        hints:      " deep down nested cmd",
        markup: "IIIV",
        cursor: 4,
        current: "__command",
        status: "ERROR",
        unassigned: [ ]
      }
    },
    {
      skipIf: options.isPhantomjs, // PhantomJS gets predictions wrong
      setup:    "tsn x",
      check: {
        input:  "tsn x",
        hints:       " -> tsn ext",
        markup: "IIIVI",
        cursor: 5,
        current: "__command",
        status: "ERROR",
        predictions: [ "tsn ext" ],
        unassigned: [ ]
      }
    },
    {
      setup:    "tsn dif",
      check: {
        input:  "tsn dif",
        hints:         " <text>",
        markup: "VVVVVVV",
        cursor: 7,
        current: "__command",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsn dif" },
          text: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018text\u2019."
          }
        }
      }
    },
    {
      setup:    "tsn dif ",
      check: {
        input:  "tsn dif ",
        hints:          "<text>",
        markup: "VVVVVVVV",
        cursor: 8,
        current: "text",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsn dif" },
          text: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018text\u2019."
          }
        }
      }
    },
    {
      setup:    "tsn dif x",
      check: {
        input:  "tsn dif x",
        hints:           "",
        markup: "VVVVVVVVV",
        cursor: 9,
        current: "text",
        status: "VALID",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsn dif" },
          text: { value: "x", arg: " x", status: "VALID", message: "" }
        }
      }
    },
    {
      setup:    "tsn ext",
      check: {
        input:  "tsn ext",
        hints:         " <text>",
        markup: "VVVVVVV",
        cursor: 7,
        current: "__command",
        status: "ERROR",
        predictions: [ "tsn ext", "tsn exte", "tsn exten", "tsn extend" ],
        unassigned: [ ],
        args: {
          command: { name: "tsn ext" },
          text: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018text\u2019."
          }
        }
      }
    },
    {
      setup:    "tsn exte",
      check: {
        input:  "tsn exte",
        hints:          " <text>",
        markup: "VVVVVVVV",
        cursor: 8,
        current: "__command",
        status: "ERROR",
        predictions: [ "tsn exte", "tsn exten", "tsn extend" ],
        unassigned: [ ],
        args: {
          command: { name: "tsn exte" },
          text: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018text\u2019."
          }
        }
      }
    },
    {
      setup:    "tsn exten",
      check: {
        input:  "tsn exten",
        hints:           " <text>",
        markup: "VVVVVVVVV",
        cursor: 9,
        current: "__command",
        status: "ERROR",
        predictions: [ "tsn exten", "tsn extend" ],
        unassigned: [ ],
        args: {
          command: { name: "tsn exten" },
          text: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018text\u2019."
          }
        }
      }
    },
    {
      setup:    "tsn extend",
      check: {
        input:  "tsn extend",
        hints:            " <text>",
        markup: "VVVVVVVVVV",
        cursor: 10,
        current: "__command",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsn extend" },
          text: {
            value: undefined,
            arg: "",
            status: "INCOMPLETE",
            message: "Value required for \u2018text\u2019."
          }
        }
      }
    },
    {
      setup:    "ts ",
      check: {
        input:  "ts ",
        hints:     "",
        markup: "EEV",
        cursor: 3,
        current: "__command",
        status: "ERROR",
        predictions: [ ],
        unassigned: [ ],
        tooltipState: "true:isError"
      }
    },
  ]);
};

// From Bug 664203
exports.testDeeplyNested = function (options) {
  return helpers.audit(options, [
    {
      setup:    "tsn deep down nested",
      check: {
        input:  "tsn deep down nested",
        hints:                      " cmd",
        markup: "IIIVIIIIVIIIIVIIIIII",
        cursor: 20,
        current: "__command",
        status: "ERROR",
        predictions: [ "tsn deep down nested cmd" ],
        unassigned: [ ],
        outputState: "false:default",
        tooltipState: "false:default",
        args: {
          command: { name: "tsn deep down nested" },
        }
      }
    },
    {
      setup:    "tsn deep down nested cmd",
      check: {
        input:  "tsn deep down nested cmd",
        hints:                          "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVV",
        cursor: 24,
        current: "__command",
        status: "VALID",
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: "tsn deep down nested cmd" },
        }
      }
    }
  ]);
};
