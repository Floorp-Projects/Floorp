/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
// Throwing message listener
onmessage = function(event) {
  throw new Error("Bah!");
};
