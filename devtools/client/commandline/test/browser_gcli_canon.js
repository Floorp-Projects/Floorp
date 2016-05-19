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
  helpers.runTestModule(exports, "browser_gcli_canon.js");
}

// var assert = require('../testharness/assert');
// var helpers = require('./helpers');
var Commands = require("gcli/commands/commands").Commands;

var startCount;
var events;

var commandsChange = function (ev) {
  events++;
};

exports.setup = function (options) {
  startCount = options.requisition.system.commands.getAll().length;
  events = 0;
};

exports.shutdown = function (options) {
  startCount = undefined;
  events = undefined;
};

exports.testAddRemove1 = function (options) {
  var commands = options.requisition.system.commands;

  return helpers.audit(options, [
    {
      name: "testadd add",
      setup: function () {
        commands.onCommandsChange.add(commandsChange);

        commands.add({
          name: "testadd",
          exec: function () {
            return 1;
          }
        });

        assert.is(commands.getAll().length,
                  startCount + 1,
                  "add command success");
        assert.is(events, 1, "add event");

        return helpers.setInput(options, "testadd");
      },
      check: {
        input:  "testadd",
        hints:         "",
        markup: "VVVVVVV",
        cursor: 7,
        current: "__command",
        status: "VALID",
        predictions: [ ],
        unassigned: [ ],
        args: { }
      },
      exec: {
        output: /^1$/
      }
    },
    {
      name: "testadd alter",
      setup: function () {
        commands.add({
          name: "testadd",
          exec: function () {
            return 2;
          }
        });

        assert.is(commands.getAll().length,
                  startCount + 1,
                  "read command success");
        assert.is(events, 2, "read event");

        return helpers.setInput(options, "testadd");
      },
      check: {
        input:  "testadd",
        hints:         "",
        markup: "VVVVVVV",
      },
      exec: {
        output: "2"
      }
    },
    {
      name: "testadd remove",
      setup: function () {
        commands.remove("testadd");

        assert.is(commands.getAll().length,
                  startCount,
                  "remove command success");
        assert.is(events, 3, "remove event");

        return helpers.setInput(options, "testadd");
      },
      check: {
        typed: "testadd",
        cursor: 7,
        current: "__command",
        status: "ERROR",
        unassigned: [ ],
      }
    }
  ]);
};

exports.testAddRemove2 = function (options) {
  var commands = options.requisition.system.commands;

  commands.add({
    name: "testadd",
    exec: function () {
      return 3;
    }
  });

  assert.is(commands.getAll().length,
            startCount + 1,
            "rereadd command success");
  assert.is(events, 4, "rereadd event");

  return helpers.audit(options, [
    {
      setup: "testadd",
      exec: {
        output: /^3$/
      },
      post: function () {
        commands.remove({
          name: "testadd"
        });

        assert.is(commands.getAll().length,
                  startCount,
                  "reremove command success");
        assert.is(events, 5, "reremove event");
      }
    },
    {
      setup: "testadd",
      check: {
        typed: "testadd",
        status: "ERROR"
      }
    }
  ]);
};

exports.testAddRemove3 = function (options) {
  var commands = options.requisition.system.commands;

  commands.remove({ name: "nonexistant" });
  assert.is(commands.getAll().length,
            startCount,
            "nonexistant1 command success");
  assert.is(events, 5, "nonexistant1 event");

  commands.remove("nonexistant");
  assert.is(commands.getAll().length,
            startCount,
            "nonexistant2 command success");
  assert.is(events, 5, "nonexistant2 event");

  commands.onCommandsChange.remove(commandsChange);
};

exports.testAltCommands = function (options) {
  var commands = options.requisition.system.commands;
  var altCommands = new Commands(options.requisition.system.types);

  var tss = {
    name: "tss",
    params: [
      { name: "str", type: "string" },
      { name: "num", type: "number" },
      { name: "opt", type: { name: "selection", data: [ "1", "2", "3" ] } },
    ],
    customProp1: "localValue",
    customProp2: true,
    customProp3: 42,
    exec: function (args, context) {
      return context.commandName + ":" +
              args.str + ":" + args.num + ":" + args.opt;
    }
  };
  altCommands.add(tss);

  var commandSpecs = altCommands.getCommandSpecs();
  assert.is(JSON.stringify(commandSpecs),
            '[{"item":"command","name":"tss","params":[' +
              '{"name":"str","type":"string"},' +
              '{"name":"num","type":"number"},' +
              '{"name":"opt","type":{"name":"selection","data":["1","2","3"]}}' +
            '],"isParent":false}]',
            "JSON.stringify(commandSpecs)");

  var customProps = [ "customProp1", "customProp2", "customProp3", ];
  var commandSpecs2 = altCommands.getCommandSpecs(customProps);
  assert.is(JSON.stringify(commandSpecs2),
            "[{" +
              '"item":"command",' +
              '"name":"tss",' +
              '"params":[' +
                '{"name":"str","type":"string"},' +
                '{"name":"num","type":"number"},' +
                '{"name":"opt","type":{"name":"selection","data":["1","2","3"]}}' +
              "]," +
              '"isParent":false,' +
              '"customProp1":"localValue",' +
              '"customProp2":true,' +
              '"customProp3":42' +
            "}]",
            "JSON.stringify(commandSpecs)");

  var remoter = function (args, context) {
    assert.is(context.commandName, "tss", "commandName is tss");

    var cmd = altCommands.get(context.commandName);
    return cmd.exec(args, context);
  };

  commands.addProxyCommands(commandSpecs, remoter, "proxy", "test");

  var parent = commands.get("proxy");
  assert.is(parent.name, "proxy", "Parent command called proxy");

  var child = commands.get("proxy tss");
  assert.is(child.name, "proxy tss", "child command called proxy tss");

  return helpers.audit(options, [
    {
      setup:    "proxy tss foo 6 3",
      check: {
        input:  "proxy tss foo 6 3",
        hints:                    "",
        markup: "VVVVVVVVVVVVVVVVV",
        cursor: 17,
        status: "VALID",
        args: {
          str: { value: "foo", status: "VALID" },
          num: { value: 6, status: "VALID" },
          opt: { value: "3", status: "VALID" }
        }
      },
      exec: {
        output: "tss:foo:6:3"
      },
      post: function () {
        commands.remove("proxy");
        commands.remove("proxy tss");

        assert.is(commands.get("proxy"), undefined, "remove proxy");
        assert.is(commands.get("proxy tss"), undefined, "remove proxy tss");
      }
    }
  ]);
};
