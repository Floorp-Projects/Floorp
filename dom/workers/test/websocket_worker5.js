importScripts('../../../dom/base/test/websocket_helpers.js');
importScripts('../../../dom/base/test/websocket_tests.js');
importScripts('websocket_helpers.js');

var tests = [
  test42, // non-char utf-8 sequences
  test43, // Test setting binaryType attribute
  test44, // Test sending/receving binary ArrayBuffer
  test46, // Test that we don't dispatch incoming msgs once in CLOSING state
  test47, // Make sure onerror/onclose aren't called during close()
  test49, // Test that we fail if subprotocol returned from server doesn't match 
];

doTest();
