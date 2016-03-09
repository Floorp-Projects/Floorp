importScripts('../../../dom/base/test/websocket_helpers.js');
importScripts('../../../dom/base/test/websocket_tests.js');
importScripts('websocket_helpers.js');

var tests = [
 test24, // server rejects sub-protocol string
 test25, // ctor with valid empty sub-protocol array
 test26, // ctor with invalid sub-protocol array containing 1 empty element
 test27, // ctor with invalid sub-protocol array containing an empty element in
         // list
 test28, // ctor using valid 1 element sub-protocol array
 test29, // ctor using all valid 5 element sub-protocol array
 test30, // ctor using valid 1 element sub-protocol array with element server
         // will reject
];

doTest();
