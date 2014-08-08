/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

onmessage = function(event) {
  postMessage({event: 'console exists', status: !!console, last : false});
  console = 42;
  postMessage({event: 'console is replaceable', status: console === 42, last : true});
}
