/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const Assert = {
  ok(value, message) {
    postMessage({
      op: "ok",
      value: !!value,
      message,
    });
  },
  equal(a, b, message) {
    postMessage({
      op: "is",
      a,
      b,
      message,
    });
  },
};
