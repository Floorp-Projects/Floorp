/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onerror = function(message, filename, lineno) {
  throw new Error("2");
};

onmessage = function(event) {
  throw new Error("1");
};
