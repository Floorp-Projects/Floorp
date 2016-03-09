importScripts('../../../dom/base/test/websocket_helpers.js');
importScripts('../../../dom/base/test/websocket_tests.js');
importScripts('websocket_helpers.js');

var tests = [
  test1,  // client tries to connect to a http scheme location;
  test2,  // assure serialization of the connections;
  test3,  // client tries to connect to an non-existent ws server;
  test4,  // client tries to connect using a relative url;
  test5,  // client uses an invalid protocol value;
  test6,  // counter and encoding check;
  test7,  // onmessage event origin property check
  test8,  // client calls close() and the server sends the close frame (with no
          // code or reason) in acknowledgement;
  test9,  // client closes the connection before the ws connection is established;
  test10, // client sends a message before the ws connection is established;
];

doTest();
