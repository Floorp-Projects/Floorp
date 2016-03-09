importScripts('../../../dom/base/test/websocket_helpers.js');
importScripts('../../../dom/base/test/websocket_tests.js');
importScripts('websocket_helpers.js');

var tests = [
  test31, // ctor using valid 2 element sub-protocol array with 1 element server
          // will reject and one server will accept
  test32, // ctor using invalid sub-protocol array that contains duplicate items
  test33, // test for sending/receiving custom close code (but no close reason)
  test34, // test for receiving custom close code and reason
  test35, // test for sending custom close code and reason
  test36, // negative test for sending out of range close code
  test37, // negative test for too long of a close reason
  test38, // ensure extensions attribute is defined
  test39, // a basic wss:// connectivity test
  test40, // negative test for wss:// with no cert
];

doTest();
