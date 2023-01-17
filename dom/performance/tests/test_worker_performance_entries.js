function ok(a, msg) {
  postMessage({ type: "check", status: !!a, msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish(a, msg) {
  postMessage({ type: "finish" });
}

async function wait_for_performance_entries() {
  let promise = new Promise(resolve => {
    new PerformanceObserver(list => {
      resolve(list.getEntries());
    }).observe({ entryTypes: ["resource"] });
  });
  entries = await promise;
  return entries;
}

async function check(resource, initiatorType, protocol) {
  let entries = performance.getEntries();
  if (!entries.length) {
    entries = await wait_for_performance_entries();
  }
  ok(entries.length == 1, "We have an entry");

  ok(entries[0] instanceof PerformanceEntry, "The entry is a PerformanceEntry");
  ok(entries[0].name.endsWith(resource), "The entry has been found!");

  is(entries[0].entryType, "resource", "Correct EntryType");
  ok(entries[0].startTime > 0, "We have a startTime");
  ok(entries[0].duration > 0, "We have a duration");

  ok(
    entries[0] instanceof PerformanceResourceTiming,
    "The entry is a PerformanceResourceTiming"
  );

  is(entries[0].initiatorType, initiatorType, "Correct initiatorType");
  is(entries[0].nextHopProtocol, protocol, "Correct protocol");

  performance.clearResourceTimings();
}

function simple_checks() {
  ok("performance" in self, "We have self.performance");
  performance.clearResourceTimings();
  next();
}

function fetch_request() {
  fetch("test_worker_performance_entries.sjs")
    .then(r => r.blob())
    .then(blob => {
      check("test_worker_performance_entries.sjs", "fetch", "http/1.1");
    })
    .then(next);
}

function xhr_request() {
  let xhr = new XMLHttpRequest();
  xhr.open("GET", "test_worker_performance_entries.sjs");
  xhr.send();
  xhr.onload = () => {
    check("test_worker_performance_entries.sjs", "xmlhttprequest", "http/1.1");
    next();
  };
}

function sync_xhr_request() {
  let xhr = new XMLHttpRequest();
  xhr.open("GET", "test_worker_performance_entries.sjs", false);
  xhr.send();
  check("test_worker_performance_entries.sjs", "xmlhttprequest", "http/1.1");
  next();
}

function import_script() {
  importScripts(["empty.js"]);
  check("empty.js", "other", "http/1.1");
  next();
}

function redirect() {
  fetch("test_worker_performance_entries.sjs?redirect")
    .then(r => r.text())
    .then(async text => {
      is(text, "Hello world \\o/", "The redirect worked correctly");
      await check(
        "test_worker_performance_entries.sjs?redirect",
        "fetch",
        "http/1.1"
      );
    })
    .then(next);
}

let tests = [
  simple_checks,
  fetch_request,
  xhr_request,
  sync_xhr_request,
  import_script,
  redirect,
];

function next() {
  if (!tests.length) {
    finish();
    return;
  }

  let test = tests.shift();
  test();
}

next();
