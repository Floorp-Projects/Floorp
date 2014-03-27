/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Loader } = require("sdk/test/loader");
const { isWeak, WeakReference } = require("sdk/core/reference");
const { subscribe, unsubscribe,
        observe, Observer } = require("sdk/core/observer");
const { Class } = require("sdk/core/heritage");

const { Cc, Ci, Cu } = require("chrome");
const { notifyObservers } = Cc["@mozilla.org/observer-service;1"].
                              getService(Ci.nsIObserverService);

const message = x => ({wrappedJSObject: x});

exports["test subscribe unsubscribe"] = assert => {
  const topic = Date.now().toString(32);
  const Subscriber = Class({
    extends: Observer,
    initialize: function(observe) {
      this.observe = observe;
    }
  });
  observe.define(Subscriber, (x, subject, _, data) =>
                                x.observe(subject.wrappedJSObject.x));

  let xs = [];
  const x = Subscriber((...rest) => xs.push(...rest));

  let ys = [];
  const y = Subscriber((...rest) => ys.push(...rest));

  const publish = (topic, data) =>
    notifyObservers(message(data), topic, null);

  publish({x:0});

  subscribe(x, topic);

  publish(topic, {x:1});

  subscribe(y, topic);

  publish(topic, {x:2});
  publish(topic + "!", {x: 2.5});

  unsubscribe(x, topic);

  publish(topic, {x:3});

  subscribe(y, topic);

  publish(topic, {x:4});

  subscribe(x, topic);

  publish(topic, {x:5});

  unsubscribe(x, topic);
  unsubscribe(y, topic);

  publish(topic, {x:6});

  assert.deepEqual(xs, [1, 2, 5]);
  assert.deepEqual(ys, [2, 3, 4, 5]);
}

exports["test weak observers are GC-ed on unload"] = (assert, end) => {
  const topic = Date.now().toString(32);
  const loader = Loader(module);
  const { Observer, observe,
          subscribe, unsubscribe } = loader.require("sdk/core/observer");
  const { isWeak, WeakReference } = loader.require("sdk/core/reference");

  const MyObserver = Class({
    extends: Observer,
    initialize: function(observe) {
      this.observe = observe;
    }
  });
  observe.define(MyObserver, (x, ...rest) => x.observe(...rest));

  const MyWeakObserver = Class({
    extends: MyObserver,
    implements: [WeakReference]
  });

  let xs = [];
  let ys = [];
  let x = new MyObserver((subject, topic, data) => {
    xs.push(subject.wrappedJSObject, topic, data);
  });
  let y = new MyWeakObserver((subject, topic, data) => {
    ys.push(subject.wrappedJSObject, topic, data);
  });

  subscribe(x, topic);
  subscribe(y, topic);


  notifyObservers(message({ foo: 1 }), topic, null);
  x = null;
  y = null;
  loader.unload();

  Cu.schedulePreciseGC(() => {

    notifyObservers(message({ bar: 2 }), topic, ":)");

    assert.deepEqual(xs, [{ foo: 1 }, topic, null,
                          { bar: 2 }, topic, ":)"],
                     "non week observer is kept");

    assert.deepEqual(ys, [{ foo: 1 }, topic, null],
                     "week observer was GC-ed");

    end();
  });
};

require("sdk/test").run(exports);