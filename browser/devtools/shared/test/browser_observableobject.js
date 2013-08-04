/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let tmp = {};
  Cu.import("resource://gre/modules/devtools/Loader.jsm", tmp);
  let {ObservableObject} = tmp.devtools.require("devtools/shared/observable-object");

  let rawObject = {};
  let oe = new ObservableObject(rawObject);

  function str(o) {
    return JSON.stringify(o);
  }

  function areObjectsSynced() {
    is(str(rawObject), str(oe.object), "Objects are synced");
  }

  areObjectsSynced();

  let index = 0;
  let expected = [
    {type: "set", path: "foo", value: 4},
    {type: "get", path: "foo", value: 4},
    {type: "get", path: "foo", value: 4},
    {type: "get", path: "bar", value: undefined},
    {type: "get", path: "bar", value: undefined},
    {type: "set", path: "bar", value: {}},
    {type: "get", path: "bar", value: {}},
    {type: "get", path: "bar", value: {}},
    {type: "set", path: "bar.a", value: [1,2,3,4]},
    {type: "get", path: "bar", value: {a:[1,2,3,4]}},
    {type: "set", path: "bar.mop", value: 1},
    {type: "set", path: "bar", value: {}},
    {type: "set", path: "foo", value: [{a:42}]},
    {type: "get", path: "foo", value: [{a:42}]},
    {type: "get", path: "foo.0", value: {a:42}},
    {type: "get", path: "foo.0.a", value: 42},
    {type: "get", path: "foo", value: [{a:42}]},
    {type: "get", path: "foo.0", value: {a:42}},
    {type: "set", path: "foo.0.a", value: 2},
  ];

  function callback(event, path, value) {
    oe.off("get", callback);
    let e = expected[index];
    is(event, e.type, "[" + index + "] Right event received");
    is(path.join("."), e.path, "[" + index + "] Path valid");
    is(str(value), str(e.value), "[" + index + "] Value valid");
    index++;
    areObjectsSynced();
    oe.on("get", callback);
    if (index == expected.length) {
      finish();
    }
  }

  oe.on("set", callback);
  oe.on("get", callback);

  oe.object.foo = 4;
  oe.object.foo;
  Object.getOwnPropertyDescriptor(oe.object, "foo")
  oe.object["bar"];
  oe.object.bar;
  oe.object.bar = {};
  oe.object.bar;
  oe.object.bar.a = [1,2,3,4];
  Object.defineProperty(oe.object.bar, "mop", {value:1});
  oe.object.bar = {};
  oe.object.foo = [{a:42}];
  oe.object.foo[0].a;
  oe.object.foo[0].a = 2;
}
