importScripts('../../../dom/base/test/websocket_helpers.js');
importScripts('../../../dom/base/test/websocket_tests.js');
importScripts('websocket_helpers.js');

var tests = [
  test11, // a simple hello echo;
  test12, // client sends a message containing unpaired surrogates
  test13, //server sends an invalid message;
  test14, // server sends the close frame, it doesn't close the tcp connection
          // and it keeps sending normal ws messages;
  test15, // server closes the tcp connection, but it doesn't send the close
          // frame;
  test16, // client calls close() and tries to send a message;
  test18, // client tries to connect to an http resource;
  test19, // server closes the tcp connection before establishing the ws
          // connection;
];

doTest();
