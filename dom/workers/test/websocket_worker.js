/*
 * tests:
 *  1. client tries to connect to a http scheme location;
 *  2. assure serialization of the connections;
 *  3. client tries to connect to an non-existent ws server;
 *  4. client tries to connect using a relative url;
 *  5. client uses an invalid protocol value;
 *  6. counter and encoding check;
 *  7. onmessage event origin property check
 *  8. client calls close() and the server sends the close frame (with no code
 *     or reason) in acknowledgement;
 *  9. client closes the connection before the ws connection is established;
 * 10. client sends a message before the ws connection is established;
 * 11. a simple hello echo;
 * 12. client sends a message containing unpaired surrogates
 * 13. server sends an invalid message;
 * 14. server sends the close frame, it doesn't close the tcp connection and
 *     it keeps sending normal ws messages;
 * 15. server closes the tcp connection, but it doesn't send the close frame;
 * 16. client calls close() and tries to send a message;
 * 17. see bug 572975 - all event listeners set - Only in the main thread tests (requires forcegc)
 * 18. client tries to connect to an http resource;
 * 19. server closes the tcp connection before establishing the ws connection;
 * 20. see bug 572975 - only on error and onclose event listeners set - Only in the main thread worker tests (requires forcegc)
 * 21. see bug 572975 - same as test 17, but delete strong event listeners when
 *     receiving the message event - Only in the main thread worker tests (requires forcegc and window)
 * 22. server takes too long to establish the ws connection - Only in the main thread worker tests (requires SpecialPower object)
 * 23. should detect WebSocket on window object - Only in the main thread worker tests (requires window object)
 * 24. server rejects sub-protocol string
 * 25. ctor with valid empty sub-protocol array
 * 26. ctor with invalid sub-protocol array containing 1 empty element
 * 27. ctor with invalid sub-protocol array containing an empty element in list
 * 28. ctor using valid 1 element sub-protocol array
 * 29. ctor using all valid 5 element sub-protocol array
 * 30. ctor using valid 1 element sub-protocol array with element server will
 *     reject
 * 31. ctor using valid 2 element sub-protocol array with 1 element server
 *     will reject and one server will accept.
 * 32. ctor using invalid sub-protocol array that contains duplicate items
 * 33. test for sending/receiving custom close code (but no close reason)
 * 34. test for receiving custom close code and reason
 * 35. test for sending custom close code and reason
 * 36. negative test for sending out of range close code
 * 37. negative test for too long of a close reason
 * 38. ensure extensions attribute is defined
 * 39. a basic wss:// connectivity test
 * 40. negative test for wss:// with no cert
 * 41. HSTS
 * 42. non-char utf-8 sequences
 * 43. Test setting binaryType attribute
 * 44. Test sending/receiving binary ArrayBuffer
 * 45. Test sending/receiving binary Blob  - Only in the main thread worker tests (requires SpecialPower object)
 * 46. Test that we don't dispatch incoming msgs once in CLOSING state
 * 47. Make sure onerror/onclose aren't called during close()
 */

function feedback(msg)
{
    postMessage({type: 'feedback', msg: msg });
}

function ok(status, msg)
{
  postMessage({type: 'status', status: !!status, msg: msg});
}

function is(a, b, msg)
{
  ok(a == b, msg);
}

var first_test = 1;
var last_test = 47;


// Set this to >1 if you want to run the suite multiple times to probe for
// random orange failures. 
// - Do NOT check into mozilla-central with a value != 1.
// - Too large a count will wind up causing tryserver to timeout the test (which
//   is ok, but means all testruns will be orange).  If I set first_test to >22
//   (i.e don't run any of the tests that require waiting) I can get ~250-300
//   iterations of the remaining tests w/o a timeout.
var testsuite_iterations = 1;   


var current_test = first_test;
var testsuite_iteration = 1;

var test_started = new Array(last_test);
var all_ws = [];

function shouldNotOpen(e)
{
  var ws = e.target;
  ok(false, "onopen shouldn't be called on test " + ws._testNumber + "!");
}

function shouldNotReceiveCloseEvent(e)
{
  var ws = e.target;
  var extendedErrorInfo = "";
  if (!ws._testNumber) {
    extendedErrorInfo += "\nws members:\n";
    for (var i in ws) {
      extendedErrorInfo += (i + ": " + ws[i] + "\n");
    }

    extendedErrorInfo += "\ne members:\n";
    for (var i in e) {
      extendedErrorInfo += (i + ": " + e[i] + "\n");
    }
  }
  
  // FIXME: see bug 578276. This should be a test failure, but it's too flaky on the tbox.
  ok(true, "onclose shouldn't be called on test " + ws._testNumber + "!" + extendedErrorInfo);
}

function shouldCloseCleanly(e)
{
  var ws = e.target;
  ok(e.wasClean, "the ws connection in test " + ws._testNumber + " should be closed cleanly");
}

function shouldCloseNotCleanly(e)
{
  var ws = e.target;
  ok(!e.wasClean, "the ws connection in test " + ws._testNumber + " shouldn't be closed cleanly");
}

function ignoreError(e)
{
}

function CreateTestWS(ws_location, ws_protocol, no_increment_test)
{
  var ws;

  try {
    if (ws_protocol == undefined) {
      ws = new WebSocket(ws_location);
    } else {
      ws = new WebSocket(ws_location, ws_protocol);
    }


    ws._testNumber = current_test;
    ws._receivedCloseEvent = false;
    var infoString = "Created websocket for test " + ws._testNumber;
    if (undefined != ws_protocol) {
      infoString += " " + ws_protocol;
    }
    infoString += "\n";
    ok(true, infoString);

    ws.onerror = function(e)
    {
      ok(false, "onerror called on test " + e.target._testNumber + "!");
    };
    ws.addEventListener("close", function(e)
    {
      ws._receivedCloseEvent = true;
    }, false);
  }
  catch (e) {
    throw e;
  }
  finally {
    if (!no_increment_test) {
      current_test++;
    }
  }

  all_ws.push(ws);
  return ws;
}

function doTest(number)
{
  if (number > last_test) {
    ranAllTests = true;
    maybeFinished();
    return;
  }

  if (testsuite_iteration > 1) {
    feedback("Test suite iteration #" + testsuite_iteration + " of " + testsuite_iterations +
      ": Executing test: " + number + " of " + last_test + " tests.");
  } else {
    feedback("Executing test: " + number + " of " + last_test + " tests.");
  }

  var fnTest = eval("test" + number + "");

  if (test_started[number] === true) {
    doTest(number + 1);
    return;
  }

  test_started[number] = true;
  fnTest();
}
doTest.timeoutId = null;

function test1()
{
  try {
    var ws = CreateTestWS("http://mochi.test:8888/tests/content/base/test/file_websocket");
    ok(false, "test1 failed");
  }
  catch (e) {
    ok(true, "test1 failed");
  }
  doTest(2);
}

// this test expects that the serialization list to connect to the proxy
// is empty. Use different domain so we can run this in the background
// and not delay other tests.

var waitTest2Part1 = false;
var waitTest2Part2 = false;

function test2()
{
  waitTest2Part1 = true;
  waitTest2Part2 = true;

  var ws1 = CreateTestWS("ws://sub2.test2.example.com/tests/content/base/test/file_websocket", "test-2.1");
  current_test--; // CreateTestWS incremented this
  var ws2 = CreateTestWS("ws://sub2.test2.example.com/tests/content/base/test/file_websocket", "test-2.2");

  var ws2CanConnect = false;

  // the server will delay ws1 for 5 seconds, but the other tests can
  // proceed in parallel
  doTest(3);

  ws1.onopen = function()
  {
    ok(true, "ws1 open in test 2");
    ws2CanConnect = true;
    ws1.close();
  }

  ws1.onclose = function(e)
  {
    waitTest2Part1 = false;
    maybeFinished();
  };

  ws2.onopen = function()
  {
    ok(ws2CanConnect, "shouldn't connect yet in test-2!");
    ws2.close();
  }

  ws2.onclose = function(e)
  {
    waitTest2Part2 = false;
    maybeFinished();
  };
}

function test3()
{
  var hasError = false;
  var ws = CreateTestWS("ws://this.websocket.server.probably.does.not.exist");
  ws.onopen = shouldNotOpen;
  ws.onerror = function (e)
  {
    hasError = true;
  }

  ws.onclose = function(e)
  {
    shouldCloseNotCleanly(e);
    ok(hasError, "rcvd onerror event");
    ok(e.code == 1006, "test-3 close code should be 1006 but is:" + e.code);
    doTest(4);
  };
}

function test4()
{
  try {
    var ws = CreateTestWS("file_websocket");
    ok(false, "test-4 failed");
  }
  catch (e) {
    ok(true, "test-4 failed");
  }
  doTest(5);
}

function test5()
{
  try {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "");
    ok(false, "couldn't accept an empty string in the protocol parameter");
  }
  catch (e) {
    ok(true, "couldn't accept an empty string in the protocol parameter");
  }
  current_test--; // CreateTestWS incremented this
  try {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "\n");
    ok(false, "couldn't accept any not printable ASCII character in the protocol parameter");
  }
  catch (e) {
    ok(true, "couldn't accept any not printable ASCII character in the protocol parameter");
  }
  current_test--; // CreateTestWS incremented this
  try {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test 5");
    ok(false, "U+0020 not acceptable in protocol parameter");
  }
  catch (e) {
    ok(true, "U+0020 not acceptable in protocol parameter");
  }
  doTest(6);
}

function test6()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-6");
  var counter = 1;
  ws.onopen = function()
  {
    ws.send(counter);
  }
  ws.onmessage = function(e)
  {
    if (counter == 5) {
      ok(e.data == "あいうえお", "test-6 counter 5 data ok");
      ws.close();
    } else {
      ok(e.data == counter+1, "bad counter");
      counter += 2;
      ws.send(counter);
    }
  }
  ws.onclose = function(e)
  {
    shouldCloseCleanly(e);
    doTest(7);
  };
}

function test7()
{
  var ws = CreateTestWS("ws://sub2.test2.example.org/tests/content/base/test/file_websocket", "test-7");
  var gotmsg = false;

  ws.onopen = function()
  {
    ok(true, "test 7 open");
  }
  ws.onmessage = function(e)
  {
    ok(true, "test 7 message");
    ok(e.origin == "ws://sub2.test2.example.org", "onmessage origin set to ws:// host");
    gotmsg = true;
    ws.close();
  }
  ws.onclose = function(e)
  {
    ok(gotmsg, "recvd message in test 7 before close");
    shouldCloseCleanly(e);
    doTest(8);
  };
}

function test8()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-8");
  ws.onopen = function()
  {
    ok(ws.protocol == "test-8", "test-8 subprotocol selection");
    ws.close();
  }
  ws.onclose = function(e)
  {
    shouldCloseCleanly(e);
    // We called close() with no close code: so pywebsocket will also send no
    // close code, which translates to code 1005 
    ok(e.code == 1005, "test-8 close code has wrong value:" + e.code);
    ok(e.reason == "", "test-8 close reason has wrong value:" + e.reason);
    doTest(9);
  };
}

var waitTest9 = false;

function test9()
{
  waitTest9 = true;

  var ws = CreateTestWS("ws://test2.example.org/tests/content/base/test/file_websocket", "test-9");
  ws._receivedErrorEvent = false;
  ws.onopen = shouldNotOpen;
  ws.onerror = function(e)
  {
    ws._receivedErrorEvent = true;
  };
  ws.onclose = function(e)
  {
    ok(ws._receivedErrorEvent, "Didn't received the error event in test 9.");
    shouldCloseNotCleanly(e);
    waitTest9 = false;
    maybeFinished();
  };

  ws.close();
  
  // the server injects a delay, so proceed with this in the background
  doTest(10);
}

var waitTest10 = false;

function test10()
{
  waitTest10 = true;

  var ws = CreateTestWS("ws://sub1.test1.example.com/tests/content/base/test/file_websocket", "test-10");
  ws.onclose = function(e)
  {
    shouldCloseCleanly(e);
    waitTest10 = false;
    maybeFinished();
  }

  try {
    ws.send("client data");
    ok(false, "Couldn't send data before connecting!");
  }
  catch (e) {
    ok(true, "Couldn't send data before connecting!");
  }
  ws.onopen = function()
  {
    ok(true, "test 10 opened");
    ws.close();
  }

  // proceed with this test in the background
  doTest(11);
}

function test11()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-11");
  ok(ws.readyState == 0, "create bad readyState in test-11!");
  ws.onopen = function()
  {
    ok(ws.readyState == 1, "open bad readyState in test-11!");
    ws.send("client data");
  }
  ws.onmessage = function(e)
  {
    ok(e.data == "server data", "bad received message in test-11!");
    ws.close(1000, "Have a nice day");

// this ok() is disabled due to a race condition - it state may have
// advanced through 2 (closing) and into 3 (closed) before it is evald
//    ok(ws.readyState == 2, "onmessage bad readyState in test-11!");
  }
  ws.onclose = function(e)
  {
    ok(ws.readyState == 3, "onclose bad readyState in test-11!");
    shouldCloseCleanly(e);
    ok(e.code == 1000, "test 11 got wrong close code: " + e.code);
    ok(e.reason == "Have a nice day", "test 11 got wrong close reason: " + e.reason);
    doTest(12);
  }
}

function test12()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-12");
  ws.onopen = function()
  {
    try {
      // send an unpaired surrogate
      ws._gotMessage = false;
      ws.send("a\ud800b");
      ok(true, "ok to send an unpaired surrogate");
    }
    catch (e) {
      ok(false, "shouldn't fail any more when sending an unpaired surrogate!");
    }
  }

  ws.onmessage = function(msg)
  {
    ok(msg.data == "SUCCESS", "Unpaired surrogate in UTF-16 not converted in test-12");
    ws._gotMessage = true;
    // Must support unpaired surrogates in close reason, too
    ws.close(1000, "a\ud800b");
  }

  ws.onclose = function(e)
  {
    ok(ws.readyState == 3, "onclose bad readyState in test-12!");
    ok(ws._gotMessage, "didn't receive message!");
    shouldCloseCleanly(e);
    ok(e.code == 1000, "test 12 got wrong close code: " + e.code);
    ok(e.reason == "a\ufffdb", "test 11 didn't get replacement char in close reason: " + e.reason);
    doTest(13);
  }
}

function test13()
{
    // previous versions of this test counted the number of protocol errors returned, but the 
    // protocol stack typically closes down after reporting a protocol level error - trying
    // to resync is too dangerous

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-13");
  ws._timesCalledOnError = 0;
  ws.onerror = function()
  {
    ws._timesCalledOnError++;
  }
  ws.onclose = function(e)
  {
    ok(ws._timesCalledOnError > 0, "no error events");
    doTest(14);
  }
}

function test14()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-14");
  ws.onmessage = function()
  {
    ok(false, "shouldn't received message after the server sent the close frame");
  }
  ws.onclose = function(e)
  {
    shouldCloseCleanly(e);
    // Skip test 15 for now: broken
    doTest(16);
  };
}

/*
 * DISABLED: see comments for test-15 case in file_websocket_wsh.py
 *
function test15()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-15");
  ws.onclose = function(e)
  {
    shouldCloseNotCleanly(e);
    doTest(16);
  };

  // termination of the connection might cause an error event if it happens in OPEN
  ws.onerror = function()
  {
  }

}
*/

function test16()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-16");
  ws.onopen = function()
  {
    ws.close();
    ok(!ws.send("client data"), "shouldn't send message after calling close()");
  }
  ws.onmessage = function()
  {
    ok(false, "shouldn't send message after calling close()");
  }

  ws.onerror = function()
  {
  }
  ws.onclose = function()
  {
    doTest(18);
  }
}

// The tests that expects that their websockets neither open nor close MUST
// be in the end of the tests, i.e. HERE, in order to prevent blocking the other
// tests.

function test18()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket_http_resource.txt");
  ws.onopen = shouldNotOpen;
  ws.onerror = ignoreError;
  ws.onclose = function(e)
  {
    shouldCloseNotCleanly(e);
    doTest(19);
  };
}

function test19()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-19");
  ws.onopen = shouldNotOpen;
  ws.onerror = ignoreError;
  ws.onclose = function(e)
  {
    shouldCloseNotCleanly(e);
    doTest(24);
  };
}

function test24()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-does-not-exist");
  ws.onopen = shouldNotOpen;
  ws.onclose = function(e)
  {
    shouldCloseNotCleanly(e);
    doTest(25);
  };
  ws.onerror = function()
  {
  }
}

function test25()
{
  var prots=[];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);

  // This test errors because the server requires a sub-protocol, but
  // the test just wants to ensure that the ctor doesn't generate an
  // exception
  ws.onerror = ignoreError;
  ws.onopen = shouldNotOpen;

  ws.onclose = function(e)
  {
    ok(ws.protocol == "", "test25 subprotocol selection");
    ok(true, "test 25 protocol array close");
    doTest(26);
  };
}

function test26()
{
  var prots=[""];

  try {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
    ok(false, "testing empty element sub protocol array");
  }
  catch (e) {
    ok(true, "testing empty sub element protocol array");
  }
  doTest(27);
}

function test27()
{
  var prots=["test27", ""];

  try {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
    ok(false, "testing empty element mixed sub protocol array");
  }
  catch (e) {
    ok(true, "testing empty element mixed sub protocol array");
  }
  doTest(28);
}

function test28()
{
  var prots=["test28"];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
  ws.onopen = function(e)
  {
    ok(true, "test 28 protocol array open");
    ws.close();
  };

  ws.onclose = function(e)
  {
    ok(ws.protocol == "test28", "test28 subprotocol selection");
    ok(true, "test 28 protocol array close");
    doTest(29);
  };
}

function test29()
{
  var prots=["test29a", "test29b"];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
  ws.onopen = function(e)
  {
    ok(true, "test 29 protocol array open");
    ws.close();
  };

  ws.onclose = function(e)
  {
    ok(true, "test 29 protocol array close");
    doTest(30);
  };
}

function test30()
{
  var prots=["test-does-not-exist"];
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);

  ws.onopen = shouldNotOpen;
  ws.onclose = function(e)
  {
    shouldCloseNotCleanly(e);
    doTest(31);
  };
  ws.onerror = function()
  {
  }
}

function test31()
{
  var prots=["test-does-not-exist", "test31"];
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);

  ws.onopen = function(e)
  {
    ok(true, "test 31 protocol array open");
    ws.close();
  };

  ws.onclose = function(e)
  {
    ok(ws.protocol == "test31", "test31 subprotocol selection");
    ok(true, "test 31 protocol array close");
    doTest(32);
  };
}

function test32()
{
  var prots=["test32","test32"];

  try {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
    ok(false, "testing duplicated element sub protocol array");
  }
  catch (e) {
    ok(true, "testing duplicated sub element protocol array");
  }
  doTest(33);
}

function test33()
{
  var prots=["test33"];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
  ws.onopen = function(e)
  {
    ok(true, "test 33 open");
    ws.close(3131);   // pass code but not reason
  };

  ws.onclose = function(e)
  {
    ok(true, "test 33 close");
    shouldCloseCleanly(e);
    ok(e.code == 3131, "test 33 got wrong close code: " + e.code);
    ok(e.reason === "", "test 33 got wrong close reason: " + e.reason);
    doTest(34);
  };
}

function test34()
{
  var prots=["test-34"];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
  ws.onopen = function(e)
  {
    ok(true, "test 34 open");
    ws.close();
  };

  ws.onclose = function(e)
  {
    ok(true, "test 34 close");
    ok(e.wasClean, "test 34 closed cleanly");
    ok(e.code == 1001, "test 34 custom server code");
    ok(e.reason == "going away now", "test 34 custom server reason");
    doTest(35);
  };
}

function test35()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-35a");

  ws.onopen = function(e)
  {
    ok(true, "test 35a open");
    ws.close(3500, "my code");
  };

  ws.onclose = function(e)
  {
    ok(true, "test 35a close");
    ok(e.wasClean, "test 35a closed cleanly");
    current_test--; // CreateTestWS for 35a incremented this
    var wsb = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-35b");

  wsb.onopen = function(e)
  {
    ok(true, "test 35b open");
    wsb.close();
  };

  wsb.onclose = function(e)
  {
    ok(true, "test 35b close");
    ok(e.wasClean, "test 35b closed cleanly");
    ok(e.code == 3501, "test 35 custom server code");
    ok(e.reason == "my code", "test 35 custom server reason");
    doTest(36);
  };
  }
}

function test36()
{
  var prots=["test-36"];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
  ws.onopen = function(e)
  {
    ok(true, "test 36 open");

    try {
      ws.close(13200);
      ok(false, "testing custom close code out of range");
     }
     catch (e) {
       ok(true, "testing custom close code out of range");
       ws.close(3200);
     }
  };

  ws.onclose = function(e)
  {
    ok(true, "test 36 close");
    ok(e.wasClean, "test 36 closed cleanly");
    doTest(37);
  };
}

function test37()
{
  var prots=["test-37"];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
  ws.onopen = function(e)
  {
    ok(true, "test 37 open");

    try {
	ws.close(3100,"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123");
      ok(false, "testing custom close reason out of range");
     }
     catch (e) {
       ok(true, "testing custom close reason out of range");
       ws.close(3100,"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012");
     }
  };

  ws.onclose = function(e)
  {
    ok(true, "test 37 close");
    ok(e.wasClean, "test 37 closed cleanly");

    current_test--; // CreateTestWS for 37 incremented this
    var wsb = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-37b");

    wsb.onopen = function(e)
    {
      // now test that a rejected close code and reason dont persist
      ok(true, "test 37b open");
      try {
        wsb.close(3101,"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123");
        ok(false, "testing custom close reason out of range 37b");
      }
      catch (e) {
        ok(true, "testing custom close reason out of range 37b");
        wsb.close();
     }
    }

    wsb.onclose = function(e)
    {
      ok(true, "test 37b close");
      ok(e.wasClean, "test 37b closed cleanly");

      current_test--; // CreateTestWS for 37 incremented this
      var wsc = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-37c");

      wsc.onopen = function(e)
      {
        ok(true, "test 37c open");
        wsc.close();
      }

      wsc.onclose = function(e)
      {
         ok(e.code != 3101, "test 37c custom server code not present");
         ok(e.reason == "", "test 37c custom server reason not present");
         doTest(38);  
      }
    }
  }
}

function test38()
{
  var prots=["test-38"];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
  ws.onopen = function(e)
  {
    ok(true, "test 38 open");
    ok(ws.extensions != undefined, "extensions attribute defined");
//    ok(ws.extensions == "deflate-stream", "extensions attribute deflate-stream");
    ws.close();
  };

  ws.onclose = function(e)
  {
    ok(true, "test 38 close");
    doTest(39);
  };
}

function test39()
{
  var prots=["test-39"];

  var ws = CreateTestWS("wss://example.com/tests/content/base/test/file_websocket", prots);
  status_test39 = "started";
  ws.onopen = function(e)
  {
    status_test39 = "opened";
    ok(true, "test 39 open");
    ws.close();
  };

  ws.onclose = function(e)
  {
    ok(true, "test 39 close");
    ok(status_test39 == "opened", "test 39 did open"); 
    doTest(40);
  };
}

function test40()
{
  var prots=["test-40"];

  var ws = CreateTestWS("wss://nocert.example.com/tests/content/base/test/file_websocket", prots);

  status_test40 = "started";
  ws.onerror = ignoreError;

  ws.onopen = function(e)
  {
    status_test40 = "opened";
    ok(false, "test 40 open");
    ws.close();
  };

  ws.onclose = function(e)
  {
    ok(true, "test 40 close");
    ok(status_test40 == "started", "test 40 did not open"); 
    doTest(42);
  };
}

function test42()
{
// test some utf-8 non-characters. They should be allowed in the
// websockets context. Test via round trip echo.
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-42");
  var data = ["U+FFFE \ufffe",
		"U+FFFF \uffff",
		"U+10FFFF \udbff\udfff"];
  var index = 0;

  ws.onopen = function()
  {
    ws.send(data[0]);
    ws.send(data[1]);
    ws.send(data[2]);
  }

  ws.onmessage = function(e)
  {
    ok(e.data == data[index], "bad received message in test-42! index="+index);
    index++;
    if (index == 3)
      ws.close();
  }

  ws.onclose = function(e)
  {
    doTest(43);
  }
}

function test43()
{
  var prots=["test-43"];

  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", prots);
  ws.onopen = function(e)
  {
    ok(true, "test 43 open");
    // Test binaryType setting
    ws.binaryType = "arraybuffer";
    ws.binaryType = "blob";
    ws.binaryType = "";  // illegal
    is(ws.binaryType, "blob");
    ws.binaryType = "ArrayBuffer";  // illegal
    is(ws.binaryType, "blob");
    ws.binaryType = "Blob";  // illegal
    is(ws.binaryType, "blob");
    ws.binaryType = "mcfoofluu";  // illegal
    is(ws.binaryType, "blob");
    ws.close();
  };

  ws.onclose = function(e)
  {
    ok(true, "test 43 close");
    doTest(44);
  };
}


function test44()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-44");
  ok(ws.readyState == 0, "bad readyState in test-44!");
  ws.binaryType = "arraybuffer";
  ws.onopen = function()
  {
    ok(ws.readyState == 1, "open bad readyState in test-44!");
    var buf = new ArrayBuffer(3);
    // create byte view
    var view = new Uint8Array(buf);
    view[0] = 5;
    view[1] = 0; // null byte
    view[2] = 7;
    ws.send(buf);
  }
  ws.onmessage = function(e)
  {
    ok(e.data instanceof ArrayBuffer, "Should receive an arraybuffer!");
    var view = new Uint8Array(e.data);
    ok(view.length == 2 && view[0] == 0 && view[1] ==4, "testing Reply arraybuffer" );
    ws.close();
  }
  ws.onclose = function(e)
  {
    ok(ws.readyState == 3, "onclose bad readyState in test-44!");
    shouldCloseCleanly(e);
    doTest(46);
  }
}

function test46()
{
  var ws = CreateTestWS("ws://mochi.test:8888/tests/content/base/test/file_websocket", "test-46");
  ok(ws.readyState == 0, "create bad readyState in test-46!");
  ws.onopen = function()
  {
    ok(ws.readyState == 1, "open bad readyState in test-46!");
    ws.close()
    ok(ws.readyState == 2, "close must set readyState to 2 in test-46!");
  }
  ws.onmessage = function(e)
  {
    ok(false, "received message after calling close in test-46!");
  }
  ws.onclose = function(e)
  {
    ok(ws.readyState == 3, "onclose bad readyState in test-46!");
    shouldCloseCleanly(e);
    doTest(47);
  }
}

function test47()
{
  var hasError = false;
  var ws = CreateTestWS("ws://another.websocket.server.that.probably.does.not.exist");
  ws.onopen = shouldNotOpen;

  ws.onerror = function (e)
  {
    ok(ws.readyState == 3, "test-47: readyState should be CLOSED(3) in onerror: got " 
       + ws.readyState);
    ok(!ws._withinClose, "onerror() called during close()!");
    hasError = true;
  }

  ws.onclose = function(e)
  {
    shouldCloseNotCleanly(e);
    ok(hasError, "test-47: should have called onerror before onclose");
    ok(ws.readyState == 3, "test-47: readyState should be CLOSED(3) in onclose: got " 
       + ws.readyState);
    ok(!ws._withinClose, "onclose() called during close()!");
    ok(e.code == 1006, "test-47 close code should be 1006 but is:" + e.code);
    doTest(48);
  };

  // Call close before we're connected: throws error
  // Make sure we call onerror/onclose asynchronously
  ws._withinClose = 1;
  ws.close(3333, "Closed before we were open: error");
  ws._withinClose = 0;
  ok(ws.readyState == 2, "test-47: readyState should be CLOSING(2) after close(): got "
     + ws.readyState);
}


var ranAllTests = false;

function maybeFinished()
{
  if (!ranAllTests)
    return;

  if (waitTest2Part1 || waitTest2Part2 || waitTest9 || waitTest10)
    return;

  for (i = 0; i < all_ws.length; ++i) {
    if (all_ws[i] != shouldNotReceiveCloseEvent &&
        !all_ws[i]._receivedCloseEvent) {
      ok(false, "didn't called close on test " + all_ws[i]._testNumber + "!");
    }
  }

  if (testsuite_iteration++ < testsuite_iterations) {
    // play it again, Sam...
    ok(1, "starting testsuite iteration " + testsuite_iteration);
    test_started = new Array(last_test);
    doTest(current_test = first_test);
  } else {
    // all done
    postMessage({type: 'finish' });
  }
}

onmessage = function()
{
  doTest(first_test);
}
