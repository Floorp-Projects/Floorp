/* eslint-disable mozilla/no-comparison-or-assignment-inside-ok */

function ok(a, msg) {
  postMessage({ type: "status", status: !!a, msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish() {
  postMessage({ type: "finish" });
}

let url = new URL("http://example.com");
is(url.protocol, "http:", "http: expected");

url.protocol = "https:";
is(url.protocol, "https:", "http: -> https:");

url.protocol = "ftp:";
is(url.protocol, "ftp:", "https: -> ftp:");

url.protocol = "https:";
is(url.protocol, "https:", "ftp: -> https:");

finish();
