dump('loaded child cpow test\n');

Cu.importGlobalProperties(["XMLHttpRequest"]);

(function start() {
  [is_remote] = sendRpcMessage("cpows:is_remote");

  var tests = [
    parent_test,
    error_reporting_test,
    dom_test,
    xray_test,
    symbol_test,
    compartment_test,
    regexp_test,
    postmessage_test,
    sync_test,
    async_test,
    rpc_test,
    lifetime_test,
    cancel_test,
    cancel_test2,
    dead_test,
    unsafe_test,
  ];

  function go() {
    if (tests.length == 0) {
      sendRpcMessage("cpows:done", {});
      return;
    }

    var test = tests[0];
    tests.shift();
    test(function() {
      go();
    });
  }

  go();
})();

function ok(condition, message) {
  dump('condition: ' + condition  + ', ' + message + '\n');
  if (!condition) {
    sendAsyncMessage("cpows:fail", { message: message });
  }
}

var sync_obj;
var async_obj;

function make_object()
{
  let o = { };
  o.i = 5;
  o.b = true;
  o.s = "hello";
  o.x = { i: 10 };
  o.f = function () { return 99; };
  o.ctor = function() { this.a = 3; }

  // Doing anything with this Proxy will throw.
  var throwing = new Proxy({}, new Proxy({}, {
      get: function (trap) { throw trap; }
    }));

  let array = [1, 2, 3];

  let for_json = { "n": 3, "a": array, "s": "hello", o: { "x": 10 } };

  let proto = { data: 42 };
  let with_proto = Object.create(proto);

  let with_null_proto = Object.create(null);

  content.document.title = "Hello, Kitty";
  return { "data": o,
           "throwing": throwing,
           "document": content.document,
           "array": array,
           "for_json": for_json,
           "with_proto": with_proto,
           "with_null_proto": with_null_proto
         };
}

function make_json()
{
  return { check: "ok" };
}

function parent_test(finish)
{
  function f(check_func) {
    // Make sure this doesn't crash.
    let array = new Uint32Array(10);
    content.crypto.getRandomValues(array);

    let result = check_func(10);
    ok(result == 20, "calling function in parent worked");
    return result;
  }

  addMessageListener("cpows:from_parent", (msg) => {
    let obj = msg.objects.obj;
    if (is_remote) {
      ok(obj.a == undefined, "__exposedProps__ should not work");
    } else {
      // The same process test is not run as content, so the field can
      // be accessed even though __exposedProps__ has been removed.
      ok(obj.a == 1, "correct value from parent");
    }

    // Test that a CPOW reference to a function in the chrome process
    // is callable from unprivileged content. Greasemonkey uses this
    // functionality.
    let func = msg.objects.func;
    let sb = Cu.Sandbox('http://www.example.com', {});
    sb.func = func;
    ok(sb.eval('func()') == 101, "can call parent's function in child");

    finish();
  });
  sendRpcMessage("cpows:parent_test", {}, {func: f});
}

function error_reporting_test(finish) {
  sendRpcMessage("cpows:error_reporting_test", {}, {});
  finish();
}

function dom_test(finish)
{
  let element = content.document.createElement("div");
  element.id = "it_works";
  content.document.body.appendChild(element);

  sendRpcMessage("cpows:dom_test", {}, {element: element});
  Cu.schedulePreciseGC(function() {
    sendRpcMessage("cpows:dom_test_after_gc");
    finish();
  });
}

function xray_test(finish)
{
  let element = content.document.createElement("div");
  element.wrappedJSObject.foo = "hello";

  sendRpcMessage("cpows:xray_test", {}, {element: element});
  finish();
}

function symbol_test(finish)
{
  let iterator = Symbol.iterator;
  let named = Symbol.for("cpow-test");

  let object = {
    [iterator]: iterator,
    [named]: named,
  };
  let test = ['a'];
  sendRpcMessage("cpows:symbol_test", {}, {object: object, test: test});
  finish();
}

// Parent->Child references should go X->parent.privilegedJunkScope->child.privilegedJunkScope->Y
// Child->Parent references should go X->child.privilegedJunkScope->parent.unprivilegedJunkScope->Y
function compartment_test(finish)
{
  // This test primarily checks various compartment invariants for CPOWs, and
  // doesn't make sense to run in-process.
  if (!is_remote) {
    finish();
    return;
  }

  let sb = Cu.Sandbox('http://www.example.com', { wantGlobalProperties: ['XMLHttpRequest'] });
  sb.eval('function getUnprivilegedObject() { var xhr = new XMLHttpRequest(); xhr.expando = 42; return xhr; }');
  function testParentObject(obj) {
    let results = [];
    function is(a, b, msg) { results.push({ result: a === b ? "PASS" : "FAIL", message: msg }) };
    function ok(x, msg) { results.push({ result: x ? "PASS" : "FAIL", message: msg }) };

    let cpowLocation = Cu.getRealmLocation(obj);
    ok(/shared JSM global/.test(cpowLocation),
       "child->parent CPOWs should live in the privileged junk scope: " + cpowLocation);
    is(obj(), 42, "child->parent CPOW is invokable");
    try {
      obj.expando;
      ok(false, "child->parent CPOW cannot access properties");
    } catch (e) {
      ok(true, "child->parent CPOW cannot access properties");
    }

    return results;
  }
  sendRpcMessage("cpows:compartment_test", {}, { getUnprivilegedObject: sb.getUnprivilegedObject,
                                                 testParentObject: testParentObject });
  finish();
}

function regexp_test(finish)
{
  sendRpcMessage("cpows:regexp_test", {}, { regexp: /myRegExp/g });
  finish();
}

function postmessage_test(finish)
{
  sendRpcMessage("cpows:postmessage_test", {}, { win: content.window });
  finish();
}

function sync_test(finish)
{
  dump('beginning cpow sync test\n');
  sync_obj = make_object();
  sendRpcMessage("cpows:sync",
    make_json(),
    make_object());
  finish();
}

function async_test(finish)
{
  dump('beginning cpow async test\n');
  async_obj = make_object();
  sendAsyncMessage("cpows:async",
    make_json(),
    async_obj);

  addMessageListener("cpows:async_done", finish);
}

var rpc_obj;

function rpc_test(finish)
{
  dump('beginning cpow rpc test\n');
  rpc_obj = make_object();
  rpc_obj.data.reenter = function  () {
    sendRpcMessage("cpows:reenter", { }, { data: { valid: true } });
    return "ok";
  }
  sendRpcMessage("cpows:rpc",
    make_json(),
    rpc_obj);
  finish();
}

function lifetime_test(finish)
{
  if (!is_remote) {
    // Only run this test when running out-of-process. Otherwise it
    // will fail, since local CPOWs don't follow the same ownership
    // rules.
    finish();
    return;
  }

  dump("beginning lifetime test\n");
  var obj = {"will_die": {"f": 1}};
  let [result] = sendRpcMessage("cpows:lifetime_test_1", {}, {obj: obj});
  ok(result == 10, "got sync result");
  ok(obj.wont_die.f == undefined, "got reverse CPOW");
  obj.will_die = null;
  Cu.schedulePreciseGC(function() {
    addMessageListener("cpows:lifetime_test_3", (msg) => {
      ok(obj.wont_die.f == undefined, "reverse CPOW still works");
      finish();
    });
    sendRpcMessage("cpows:lifetime_test_2");
  });
}

function cancel_test(finish)
{
  if (!is_remote) {
    // No point in doing this in single-process mode.
    finish();
    return;
  }

  let fin1 = false, fin2 = false;

  // CPOW from the parent runs f. When it sends a sync message, the
  // CPOW is canceled. The parent starts running again immediately
  // after the CPOW is canceled; f also continues running.
  function f() {
    let res = sendSyncMessage("cpows:cancel_sync_message");
    ok(res[0] == 12, "cancel_sync_message result correct");
    fin1 = true;
    if (fin1 && fin2) finish();
  }

  sendAsyncMessage("cpows:cancel_test", null, {f: f});
  addMessageListener("cpows:cancel_test_done", msg => {
    fin2 = true;
    if (fin1 && fin2) finish();
  });
}

function cancel_test2(finish)
{
  if (!is_remote) {
    // No point in doing this in single-process mode.
    finish();
    return;
  }

  let fin1 = false, fin2 = false;

  // CPOW from the parent runs f. When it does a sync XHR, the
  // CPOW is canceled. The parent starts running again immediately
  // after the CPOW is canceled; f also continues running.
  function f() {
    let req = new XMLHttpRequest();
    let fin = false;
    let reqListener = () => {
      if (req.readyState != req.DONE) {
        return;
      }
      ok(req.status == 200, "XHR succeeded");
      fin = true;
    };

    req.onload = reqListener;
    req.open("get", "http://example.com", false);
    req.send(null);

    ok(fin == true, "XHR happened");

    fin1 = true;
    if (fin1 && fin2) finish();
  }

  sendAsyncMessage("cpows:cancel_test2", null, {f: f});
  addMessageListener("cpows:cancel_test2_done", msg => {
    fin2 = true;
    if (fin1 && fin2) finish();
  });
}

function unsafe_test(finish)
{
  if (!is_remote) {
    // Only run this test when running out-of-process.
    finish();
    return;
  }

  function f() {}

  sendAsyncMessage("cpows:unsafe", null, {f});
  addMessageListener("cpows:unsafe_done", msg => {
    sendRpcMessage("cpows:safe", null, {f});
    addMessageListener("cpows:safe_done", finish);
  });
}

function dead_test(finish)
{
  if (!is_remote) {
    // Only run this test when running out-of-process.
    finish();
    return;
  }

  let gcTrigger = function() {
    // Force the GC to dead-ify the thing.
    content.QueryInterface(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIDOMWindowUtils)
           .garbageCollect();
  }

  {
    let thing = { value: "Gonna croak" };
    sendAsyncMessage("cpows:dead", null, { thing, gcTrigger });
  }

  addMessageListener("cpows:dead_done", finish);
}
