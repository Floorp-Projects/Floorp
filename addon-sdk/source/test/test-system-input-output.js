/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";


const { id: addonID, name: addonName } = require("sdk/self");
const { Cc, Ci, Cu } = require("chrome");
const { Loader, LoaderWithHookedConsole2 } = require("sdk/test/loader");
const { InputPort } = require("sdk/input/system");
const { OutputPort } = require("sdk/output/system");

const { removeObserver, addObserver,
        notifyObservers } = Cc["@mozilla.org/observer-service;1"].
                              getService(Ci.nsIObserverService);

const { lift, start, stop, send } = require("sdk/event/utils");

const isConsoleEvent = topic =>
  ["console-api-log-event",
   "console-storage-cache-event"].indexOf(topic) >= 0;

const message = x => ({wrappedJSObject: {data: x}});

exports["test start / stop ports"] = assert => {
  const input = new InputPort({ id: Date.now().toString(32), initial: {data:0} });
  const topic = input.topic;

  assert.ok(topic.includes(addonID), "topics are namespaced to add-on");

  const xs = lift(({data}) => "x:" + data, input);
  const ys = lift(({data}) => "y:" + data, input);

  assert.deepEqual(input.value, {data:0}, "initila value is set");
  assert.deepEqual(xs.value, "x:0", "initial value is mapped");
  assert.deepEqual(ys.value, "y:0", "initial value is mapped");

  notifyObservers(message(1), topic, null);

  assert.deepEqual(input.value, {data:0}, "no message received on input port");
  assert.deepEqual(xs.value, "x:0", "no message received on xs");
  assert.deepEqual(ys.value, "y:0", "no message received on ys");

  start(xs);


  notifyObservers(message(2), topic, null);

  assert.deepEqual(input.value, {data:2}, "message received on input port");
  assert.deepEqual(xs.value, "x:2", "message received on xs");
  assert.deepEqual(ys.value, "y:2", "no message received on (not started) ys");


  notifyObservers(message(3), topic, null);


  assert.deepEqual(input.value, {data:3}, "message received on input port");
  assert.deepEqual(xs.value, "x:3", "message received on xs");
  assert.deepEqual(ys.value, "y:3", "message received on ys");


  notifyObservers(message(4), topic, null);

  assert.deepEqual(input.value, {data:4}, "message received on input port");
  assert.deepEqual(xs.value, "x:4", "message not received on (stopped) xs");
  assert.deepEqual(ys.value, "y:4", "message received on ys");


  stop(input);

  notifyObservers(message(5), topic, null);

  assert.deepEqual(input.value, {data:4}, "message note received on input port");
  assert.deepEqual(xs.value, "x:4", "message not received on (stopped) xs");
  assert.deepEqual(ys.value, "y:4", "message not received on (stopped) ys");
};

exports["test send messages to nsIObserverService"] = assert => {
  let messages = [];

  const { newURI } = Cc['@mozilla.org/network/io-service;1'].
                       getService(Ci.nsIIOService);

  const output = new OutputPort({ id: Date.now().toString(32), sync: true });
  const topic = output.topic;

  const observer = {
    QueryInterface: function() {
      return this;
    },
    observe: (subject, topic, data) => {
      // Ignores internal console events
      if (!isConsoleEvent(topic)) {
        messages.push({
          topic: topic,
          subject: subject
        });
      }
    }
  };

  addObserver(observer, topic, false);

  send(output, null);
  assert.deepEqual(messages.shift(), { topic: topic, subject: null },
                   "null message received");


  const uri = newURI("http://www.foo.com", null, null);
  send(output, uri);

  assert.deepEqual(messages.shift(), { topic: topic, subject: uri },
                   "message received");


  function customSubject() {}
  send(output, customSubject);

  let message = messages.shift();
  assert.equal(message.topic, topic, "topic was received");
  assert.equal(message.subject.wrappedJSObject, customSubject,
               "custom subject is received");

  removeObserver(observer, topic);

  send(output, { data: "more data" });

  assert.deepEqual(messages, [],
                   "no more data received");

  addObserver(observer, "*", false);

  send(output, { data: "data again" });

  message = messages.shift();
  assert.equal(message.topic, topic, "topic was received");
  assert.deepEqual(message.subject.wrappedJSObject,
                   { data: "data again" },
                   "wrapped message received");

  removeObserver(observer, "*");

  send(output, { data: "last data" });
  assert.deepEqual(messages, [],
                   "no more data received");

  assert.throws(() => send(output, "hi"),
                /Unsupproted message type: `string`/,
                "strings can't be send");

  assert.throws(() => send(output, 4),
                /Unsupproted message type: `number`/,
                "numbers can't be send");

  assert.throws(() => send(output, void(0)),
                /Unsupproted message type: `undefined`/,
                "undefineds can't be send");

  assert.throws(() => send(output, true),
                /Unsupproted message type: `boolean`/,
                "booleans can't be send");
};

exports["test async OutputPort"] = (assert, done) => {
  let async = false;
  const output = new OutputPort({ id: Date.now().toString(32) });
  const observer = {
    observe: (subject, topic, data) => {
      removeObserver(observer, topic);
      assert.equal(topic, output.topic, "correct topic");
      assert.deepEqual(subject.wrappedJSObject, {foo: "bar"}, "message received");
      assert.ok(async, "message received async");
      done();
    }
  };
  addObserver(observer, output.topic, false);
  send(output, {foo: "bar"});

  assert.throws(() => send(output, "boom"), "can only send object");
  async = true;
};

exports["test explicit output topic"] = (assert, done) => {
  const topic = Date.now().toString(32);
  const output = new OutputPort({ topic: topic });
  const observer = {
    observe: (subject, topic, data) => {
      removeObserver(observer, topic);
      assert.deepEqual(subject.wrappedJSObject, {foo: "bar"}, "message received");
      done();
    }
  };

  assert.equal(output.topic, topic, "given topic is used");

  addObserver(observer, topic, false);
  send(output, {foo: "bar"});
};

exports["test explicit input topic"] = (assert) => {
  const topic = Date.now().toString(32);
  const input = new InputPort({ topic: topic });

  start(input);
  assert.equal(input.topic, topic, "given topic is used");


  notifyObservers({wrappedJSObject: {foo: "bar"}}, topic, null);

  assert.deepEqual(input.value, {foo: "bar"}, "message received");
};


exports["test receive what was send"] = assert => {
  const id = Date.now().toString(32);
  const input = new InputPort({ id: id, initial: 0});
  const output = new OutputPort({ id: id, sync: true });

  assert.ok(input.topic.includes(addonID),
            "input topic is namespaced to addon");
  assert.equal(input.topic, output.topic,
              "input & output get same topics from id.");

  start(input);

  assert.equal(input.value, 0, "initial value is set");

  send(output, { data: 1 });
  assert.deepEqual(input.value, {data: 1}, "message unwrapped");

  send(output, []);
  assert.deepEqual(input.value, [], "array message unwrapped");

  send(output, null);
  assert.deepEqual(input.value, null, "null message received");

  send(output, new String("message"));
  assert.deepEqual(input.value, new String("message"),
                   "string instance received");

  send(output, /pattern/);
  assert.deepEqual(input.value, /pattern/, "regexp received");

  assert.throws(() => send(output, "hi"),
                /Unsupproted message type: `string`/,
                "strings can't be send");

  assert.throws(() => send(output, 4),
                /Unsupproted message type: `number`/,
                "numbers can't be send");

  assert.throws(() => send(output, void(0)),
                /Unsupproted message type: `undefined`/,
                "undefineds can't be send");

  assert.throws(() => send(output, true),
                /Unsupproted message type: `boolean`/,
                "booleans can't be send");

  stop(input);
};


exports["-test error reporting"] = function(assert) {
  let { loader, messages } = LoaderWithHookedConsole2(module);
  const { start, stop, lift } = loader.require("sdk/event/utils");
  const { InputPort } = loader.require("sdk/input/system");
  const { OutputPort } = loader.require("sdk/output/system");
  const id = "error:" + Date.now().toString(32);

  const raise = x => { if (x) throw new Error("foo"); };

  const input = new InputPort({ id: id });
  const output = new OutputPort({ id: id, sync: true });
  const xs = lift(raise, input);

  assert.equal(input.value, null, "initial inherited");

  send(output, { data: "yo yo" });

  assert.deepEqual(messages, [], "nothing happend yet");

  start(xs);

  send(output, { data: "first" });

  assert.equal(messages.length, 4, "Got an exception");


  assert.equal(messages[0], "console.error: " + addonName + ": \n",
               "error is logged");

  assert.ok(/Unhandled error/.test(messages[1]),
            "expected error message");

  loader.unload();
};

exports["test unload ends input port"] = assert => {
  const loader = Loader(module);
  const { start, stop, lift } = loader.require("sdk/event/utils");
  const { InputPort } = loader.require("sdk/input/system");

  const id = "unload!" + Date.now().toString(32);
  const input = new InputPort({ id: id });

  start(input);
  notifyObservers(message(1), input.topic, null);
  assert.deepEqual(input.value, {data: 1}, "message received");

  notifyObservers(message(2), input.topic, null);
  assert.deepEqual(input.value, {data: 2}, "message received");

  loader.unload();
  notifyObservers(message(3), input.topic, null);
  assert.deepEqual(input.value, {data: 2}, "message wasn't received");
};

require("sdk/test").run(exports);
