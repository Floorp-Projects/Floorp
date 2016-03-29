// test1: client tries to connect to a http scheme location;
function test1() {
  return new Promise(function(resolve, reject) {
    try {
      var ws = CreateTestWS("http://mochi.test:8888/tests/dom/base/test/file_websocket");
      ok(false, "test1 failed");
    } catch (e) {
      ok(true, "test1 failed");
    }

    resolve();
  });
}

// test2: assure serialization of the connections;
// this test expects that the serialization list to connect to the proxy
// is empty.
function test2() {
  return new Promise(function(resolve, reject) {
    var waitTest2Part1 = true;
    var waitTest2Part2 = true;

    var ws1 = CreateTestWS("ws://sub2.test2.example.com/tests/dom/base/test/file_websocket", "test-2.1");
    var ws2 = CreateTestWS("ws://sub2.test2.example.com/tests/dom/base/test/file_websocket", "test-2.2");

    var ws2CanConnect = false;

    function maybeFinished() {
      if (!waitTest2Part1 && !waitTest2Part2) {
        resolve();
      }
    }

    ws1.onopen = function() {
      ok(true, "ws1 open in test 2");
      ws2CanConnect = true;
      ws1.close();
    }

    ws1.onclose = function(e) {
      waitTest2Part1 = false;
      maybeFinished();
    }

    ws2.onopen = function() {
      ok(ws2CanConnect, "shouldn't connect yet in test-2!");
      ws2.close();
    }

    ws2.onclose = function(e) {
      waitTest2Part2 = false;
      maybeFinished();
    }
  });
}

// test3: client tries to connect to an non-existent ws server;
function test3() {
  return new Promise(function(resolve, reject) {
    var hasError = false;
    var ws = CreateTestWS("ws://this.websocket.server.probably.does.not.exist");

    ws.onopen = shouldNotOpen;

    ws.onerror = function (e) {
      hasError = true;
    }

    ws.onclose = function(e) {
      shouldCloseNotCleanly(e);
      ok(hasError, "rcvd onerror event");
      is(e.code, 1006, "test-3 close code should be 1006 but is:" + e.code);
      resolve();
    }
  });
}

// test4: client tries to connect using a relative url;
function test4() {
  return new Promise(function(resolve, reject) {
    try {
      var ws = CreateTestWS("file_websocket");
      ok(false, "test-4 failed");
    } catch (e) {
      ok(true, "test-4 failed");
    }

    resolve();
  });
}

// test5: client uses an invalid protocol value;
function test5() {
  return new Promise(function(resolve, reject) {
    try {
      var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "");
      ok(false, "couldn't accept an empty string in the protocol parameter");
    } catch (e) {
      ok(true, "couldn't accept an empty string in the protocol parameter");
    }

    try {
      var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "\n");
      ok(false, "couldn't accept any not printable ASCII character in the protocol parameter");
    } catch (e) {
      ok(true, "couldn't accept any not printable ASCII character in the protocol parameter");
    }

    try {
      var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test 5");
      ok(false, "U+0020 not acceptable in protocol parameter");
    } catch (e) {
      ok(true, "U+0020 not acceptable in protocol parameter");
    }

    resolve();
  });
}

// test6: counter and encoding check;
function test6() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-6");
    var counter = 1;

    ws.onopen = function() {
      ws.send(counter);
    }

    ws.onmessage = function(e) {
      if (counter == 5) {
        is(e.data, "あいうえお", "test-6 counter 5 data ok");
        ws.close();
      } else {
        is(parseInt(e.data), counter+1, "bad counter");
        counter += 2;
        ws.send(counter);
      }
    }

    ws.onclose = function(e) {
      shouldCloseCleanly(e);
      resolve();
    }
  });
}

// test7: onmessage event origin property check
function test7() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://sub2.test2.example.org/tests/dom/base/test/file_websocket", "test-7");
    var gotmsg = false;

    ws.onopen = function() {
      ok(true, "test 7 open");
    }

    ws.onmessage = function(e) {
      ok(true, "test 7 message");
      is(e.origin, "ws://sub2.test2.example.org", "onmessage origin set to ws:// host");
      gotmsg = true;
      ws.close();
    }

    ws.onclose = function(e) {
      ok(gotmsg, "recvd message in test 7 before close");
      shouldCloseCleanly(e);
      resolve();
    }
  });
}

// test8: client calls close() and the server sends the close frame (with no
//        code or reason) in acknowledgement;
function test8() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-8");

    ws.onopen = function() {
      is(ws.protocol, "test-8", "test-8 subprotocol selection");
      ws.close();
    }

    ws.onclose = function(e) {
      shouldCloseCleanly(e);
      // We called close() with no close code: so pywebsocket will also send no
      // close code, which translates to code 1005
      is(e.code, 1005, "test-8 close code has wrong value:" + e.code);
      is(e.reason, "", "test-8 close reason has wrong value:" + e.reason);
      resolve();
    }
  });
}

// test9: client closes the connection before the ws connection is established;
function test9() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://test2.example.org/tests/dom/base/test/file_websocket", "test-9");

    ws._receivedErrorEvent = false;

    ws.onopen = shouldNotOpen;

    ws.onerror = function(e) {
      ws._receivedErrorEvent = true;
    }

    ws.onclose = function(e) {
      ok(ws._receivedErrorEvent, "Didn't received the error event in test 9.");
      shouldCloseNotCleanly(e);
      resolve();
    }

    ws.close();
  });
}

// test10: client sends a message before the ws connection is established;
function test10() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://sub1.test1.example.com/tests/dom/base/test/file_websocket", "test-10");

    ws.onclose = function(e) {
      shouldCloseCleanly(e);
      resolve();
    }

    try {
      ws.send("client data");
      ok(false, "Couldn't send data before connecting!");
    } catch (e) {
      ok(true, "Couldn't send data before connecting!");
    }

    ws.onopen = function()
    {
      ok(true, "test 10 opened");
      ws.close();
    }
  });
}

// test11: a simple hello echo;
function test11() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-11");
    is(ws.readyState, 0, "create bad readyState in test-11!");

    ws.onopen = function() {
      is(ws.readyState, 1, "open bad readyState in test-11!");
      ws.send("client data");
    }

    ws.onmessage = function(e) {
      is(e.data, "server data", "bad received message in test-11!");
      ws.close(1000, "Have a nice day");

     // this ok() is disabled due to a race condition - it state may have
     // advanced through 2 (closing) and into 3 (closed) before it is evald
     // ok(ws.readyState == 2, "onmessage bad readyState in test-11!");
    }

    ws.onclose = function(e) {
      is(ws.readyState, 3, "onclose bad readyState in test-11!");
      shouldCloseCleanly(e);
      is(e.code, 1000, "test 11 got wrong close code: " + e.code);
      is(e.reason, "Have a nice day", "test 11 got wrong close reason: " + e.reason);
      resolve();
    }
  });
}

// test12: client sends a message containing unpaired surrogates
function test12() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-12");

    ws.onopen = function() {
      try {
        // send an unpaired surrogate
        ws._gotMessage = false;
        ws.send("a\ud800b");
        ok(true, "ok to send an unpaired surrogate");
      } catch (e) {
        ok(false, "shouldn't fail any more when sending an unpaired surrogate!");
      }
    }

    ws.onmessage = function(msg) {
      is(msg.data, "SUCCESS", "Unpaired surrogate in UTF-16 not converted in test-12");
      ws._gotMessage = true;
      // Must support unpaired surrogates in close reason, too
      ws.close(1000, "a\ud800b");
    }

    ws.onclose = function(e) {
      is(ws.readyState, 3, "onclose bad readyState in test-12!");
      ok(ws._gotMessage, "didn't receive message!");
      shouldCloseCleanly(e);
      is(e.code, 1000, "test 12 got wrong close code: " + e.code);
      is(e.reason, "a\ufffdb", "test 11 didn't get replacement char in close reason: " + e.reason);
      resolve();
    }
  });
}

// test13: server sends an invalid message;
function test13() {
  return new Promise(function(resolve, reject) {
    // previous versions of this test counted the number of protocol errors
    // returned, but the protocol stack typically closes down after reporting a
    // protocol level error - trying to resync is too dangerous

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-13");
    ws._timesCalledOnError = 0;

    ws.onerror = function() {
      ws._timesCalledOnError++;
    }

    ws.onclose = function(e) {
      ok(ws._timesCalledOnError > 0, "no error events");
      resolve();
    }
  });
}

// test14: server sends the close frame, it doesn't close the tcp connection
//         and it keeps sending normal ws messages;
function test14() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-14");

    ws.onmessage = function() {
      ok(false, "shouldn't received message after the server sent the close frame");
    }

    ws.onclose = function(e) {
      shouldCloseCleanly(e);
      resolve();
    };
  });
}

// test15: server closes the tcp connection, but it doesn't send the close
//         frame;
function test15() {
  return new Promise(function(resolve, reject) {
    /*
     * DISABLED: see comments for test-15 case in file_websocket_wsh.py
     */
   resolve();

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-15");
    ws.onclose = function(e) {
      shouldCloseNotCleanly(e);
      resolve();
    }

    // termination of the connection might cause an error event if it happens in OPEN
    ws.onerror = function() {
    }
  });
}

// test16: client calls close() and tries to send a message;
function test16() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-16");

    ws.onopen = function() {
      ws.close();
      ok(!ws.send("client data"), "shouldn't send message after calling close()");
    }

    ws.onmessage = function() {
      ok(false, "shouldn't send message after calling close()");
    }

    ws.onerror = function() {
    }

    ws.onclose = function() {
      resolve();
    }
  });
}

// test17: see bug 572975 - all event listeners set
function test17() {
  return new Promise(function(resolve, reject) {
    var status_test17 = "not started";

    var test17func = function() {
      var local_ws = new WebSocket("ws://sub1.test2.example.org/tests/dom/base/test/file_websocket", "test-17");
      status_test17 = "started";

      local_ws.onopen = function(e) {
        status_test17 = "opened";
        e.target.send("client data");
        forcegc();
      };

      local_ws.onerror = function() {
        ok(false, "onerror called on test " + current_test + "!");
      };

      local_ws.onmessage = function(e) {
        ok(e.data == "server data", "Bad message in test-17");
        status_test17 = "got message";
        forcegc();
      };

      local_ws.onclose = function(e) {
        ok(status_test17 == "got message", "Didn't got message in test-17!");
        shouldCloseCleanly(e);
        status_test17 = "closed";
        forcegc();
        resolve();
      };

      window._test17 = null;
      forcegc();
    }

    window._test17 = test17func;
    window._test17();
  });
}

// The tests that expects that their websockets neither open nor close MUST
// be in the end of the tests, i.e. HERE, in order to prevent blocking the other
// tests.

// test18: client tries to connect to an http resource;
function test18() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket_http_resource.txt");
    ws.onopen = shouldNotOpen;
    ws.onerror = ignoreError;
    ws.onclose = function(e)
    {
      shouldCloseNotCleanly(e);
      resolve();
    }
  });
}

// test19: server closes the tcp connection before establishing the ws
//         connection;
function test19() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-19");
    ws.onopen = shouldNotOpen;
    ws.onerror = ignoreError;
    ws.onclose = function(e)
    {
      shouldCloseNotCleanly(e);
      resolve();
    }
  });
}

// test20: see bug 572975 - only on error and onclose event listeners set
function test20() {
  return new Promise(function(resolve, reject) {
    var test20func = function() {
      var local_ws = new WebSocket("ws://sub1.test1.example.org/tests/dom/base/test/file_websocket", "test-20");

      local_ws.onerror = function() {
        ok(false, "onerror called on test " + current_test + "!");
      }

      local_ws.onclose = function(e) {
        ok(true, "test 20 closed despite gc");
        resolve();
      }

      local_ws = null;
      window._test20 = null;
      forcegc();
    }

    window._test20 = test20func;
    window._test20();
  });
}

// test21: see bug 572975 - same as test 17, but delete strong event listeners
//         when receiving the message event;
function test21() {
  return new Promise(function(resolve, reject) {
    var test21func = function() {
      var local_ws = new WebSocket("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-21");
      var received_message = false;

      local_ws.onopen = function(e) {
        e.target.send("client data");
        forcegc();
        e.target.onopen = null;
        forcegc();
      }

      local_ws.onerror = function() {
        ok(false, "onerror called on test " + current_test + "!");
      }

      local_ws.onmessage = function(e) {
        is(e.data, "server data", "Bad message in test-21");
        received_message = true;
        forcegc();
        e.target.onmessage = null;
        forcegc();
      }

      local_ws.onclose = function(e) {
        shouldCloseCleanly(e);
        ok(received_message, "close transitioned through onmessage");
        resolve();
      }

      local_ws = null;
      window._test21 = null;
      forcegc();
    }

    window._test21 = test21func;
    window._test21();
  });
}

// test22: server takes too long to establish the ws connection;
function test22() {
  return new Promise(function(resolve, reject) {
    const pref_open = "network.websocket.timeout.open";
    SpecialPowers.setIntPref(pref_open, 5);

    var ws = CreateTestWS("ws://sub2.test2.example.org/tests/dom/base/test/file_websocket", "test-22");

    ws.onopen = shouldNotOpen;
    ws.onerror = ignoreError;

    ws.onclose = function(e) {
      shouldCloseNotCleanly(e);
      resolve();
    }

    SpecialPowers.clearUserPref(pref_open);
  });
}

// test23: should detect WebSocket on window object;
function test23() {
  return new Promise(function(resolve, reject) {
    ok("WebSocket" in window, "WebSocket should be available on window object");
    resolve();
  });
}

// test24: server rejects sub-protocol string
function test24() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-does-not-exist");

    ws.onopen = shouldNotOpen;
    ws.onclose = function(e) {
      shouldCloseNotCleanly(e);
      resolve();
    }

    ws.onerror = function() {
    }
  });
}

// test25: ctor with valid empty sub-protocol array
function test25() {
  return new Promise(function(resolve, reject) {
    var prots=[];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    // This test errors because the server requires a sub-protocol, but
    // the test just wants to ensure that the ctor doesn't generate an
    // exception
    ws.onerror = ignoreError;
    ws.onopen = shouldNotOpen;

    ws.onclose = function(e) {
      is(ws.protocol, "", "test25 subprotocol selection");
      ok(true, "test 25 protocol array close");
      resolve();
    }
  });
}

// test26: ctor with invalid sub-protocol array containing 1 empty element
function test26() {
  return new Promise(function(resolve, reject) {
    var prots=[""];

    try {
      var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);
      ok(false, "testing empty element sub protocol array");
    } catch (e) {
      ok(true, "testing empty sub element protocol array");
    }

    resolve();
  });
}

// test27: ctor with invalid sub-protocol array containing an empty element in
//         list
function test27() {
  return new Promise(function(resolve, reject) {
    var prots=["test27", ""];

    try {
      var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);
      ok(false, "testing empty element mixed sub protocol array");
    } catch (e) {
      ok(true, "testing empty element mixed sub protocol array");
    }

    resolve();
  });
}

// test28: ctor using valid 1 element sub-protocol array
function test28() {
  return new Promise(function(resolve, reject) {
    var prots=["test28"];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
      ok(true, "test 28 protocol array open");
      ws.close();
    }

    ws.onclose = function(e) {
      is(ws.protocol, "test28", "test28 subprotocol selection");
      ok(true, "test 28 protocol array close");
      resolve();
    }
  });
}

// test29: ctor using all valid 5 element sub-protocol array
function test29() {
  return new Promise(function(resolve, reject) {
    var prots=["test29a", "test29b"];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
      ok(true, "test 29 protocol array open");
      ws.close();
    }

    ws.onclose = function(e) {
      ok(true, "test 29 protocol array close");
      resolve();
    }
  });
}

// test30: ctor using valid 1 element sub-protocol array with element server
//         will reject
function test30() {
  return new Promise(function(resolve, reject) {
    var prots=["test-does-not-exist"];
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = shouldNotOpen;

    ws.onclose = function(e) {
      shouldCloseNotCleanly(e);
      resolve();
    }

    ws.onerror = function() {
    }
  });
}

// test31: ctor using valid 2 element sub-protocol array with 1 element server
//         will reject and one server will accept
function test31() {
  return new Promise(function(resolve, reject) {
    var prots=["test-does-not-exist", "test31"];
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
      ok(true, "test 31 protocol array open");
      ws.close();
    }

    ws.onclose = function(e) {
      is(ws.protocol, "test31", "test31 subprotocol selection");
      ok(true, "test 31 protocol array close");
      resolve();
    }
  });
}

// test32: ctor using invalid sub-protocol array that contains duplicate items
function test32() {
  return new Promise(function(resolve, reject) {
    var prots=["test32","test32"];

    try {
      var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);
      ok(false, "testing duplicated element sub protocol array");
    } catch (e) {
      ok(true, "testing duplicated sub element protocol array");
    }

    resolve();
  });
}

// test33: test for sending/receiving custom close code (but no close reason)
function test33() {
  return new Promise(function(resolve, reject) {
    var prots=["test33"];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
      ok(true, "test 33 open");
      ws.close(3131);   // pass code but not reason
    }

    ws.onclose = function(e) {
      ok(true, "test 33 close");
      shouldCloseCleanly(e);
      is(e.code, 3131, "test 33 got wrong close code: " + e.code);
      is(e.reason, "", "test 33 got wrong close reason: " + e.reason);
      resolve();
    }
  });
}

// test34: test for receiving custom close code and reason
function test34() {
  return new Promise(function(resolve, reject) {
    var prots=["test-34"];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
      ok(true, "test 34 open");
      ws.close();
    }

    ws.onclose = function(e)
    {
      ok(true, "test 34 close");
      ok(e.wasClean, "test 34 closed cleanly");
      is(e.code, 1001, "test 34 custom server code");
      is(e.reason, "going away now", "test 34 custom server reason");
      resolve();
    }
  });
}

// test35: test for sending custom close code and reason
function test35() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-35a");

    ws.onopen = function(e) {
      ok(true, "test 35a open");
      ws.close(3500, "my code");
    }

    ws.onclose = function(e) {
      ok(true, "test 35a close");
      ok(e.wasClean, "test 35a closed cleanly");
      var wsb = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-35b");

      wsb.onopen = function(e) {
        ok(true, "test 35b open");
        wsb.close();
      }

      wsb.onclose = function(e) {
        ok(true, "test 35b close");
        ok(e.wasClean, "test 35b closed cleanly");
        is(e.code, 3501, "test 35 custom server code");
        is(e.reason, "my code", "test 35 custom server reason");
        resolve();
      }
    }
  });
}

// test36: negative test for sending out of range close code
function test36() {
  return new Promise(function(resolve, reject) {
    var prots=["test-36"];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
      ok(true, "test 36 open");

      try {
        ws.close(13200);
        ok(false, "testing custom close code out of range");
       } catch (e) {
         ok(true, "testing custom close code out of range");
         ws.close(3200);
       }
    }

    ws.onclose = function(e) {
      ok(true, "test 36 close");
      ok(e.wasClean, "test 36 closed cleanly");
      resolve();
    }
  });
}

// test37: negative test for too long of a close reason
function test37() {
  return new Promise(function(resolve, reject) {
    var prots=["test-37"];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
      ok(true, "test 37 open");

      try {
	ws.close(3100,"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123");
        ok(false, "testing custom close reason out of range");
       } catch (e) {
         ok(true, "testing custom close reason out of range");
         ws.close(3100,"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012");
       }
    }

    ws.onclose = function(e) {
      ok(true, "test 37 close");
      ok(e.wasClean, "test 37 closed cleanly");

      var wsb = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-37b");

      wsb.onopen = function(e) {
        // now test that a rejected close code and reason dont persist
        ok(true, "test 37b open");
        try {
          wsb.close(3101,"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123");
          ok(false, "testing custom close reason out of range 37b");
        } catch (e) {
          ok(true, "testing custom close reason out of range 37b");
          wsb.close();
        }
      }

      wsb.onclose = function(e) {
        ok(true, "test 37b close");
        ok(e.wasClean, "test 37b closed cleanly");

        var wsc = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-37c");

        wsc.onopen = function(e) {
          ok(true, "test 37c open");
          wsc.close();
        }

        wsc.onclose = function(e) {
          isnot(e.code, 3101, "test 37c custom server code not present");
          is(e.reason, "", "test 37c custom server reason not present");
          resolve();
        }
      }
    }
  });
}

// test38: ensure extensions attribute is defined
function test38() {
  return new Promise(function(resolve, reject) {
    var prots=["test-38"];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
      ok(true, "test 38 open");
      isnot(ws.extensions, undefined, "extensions attribute defined");
      //  is(ws.extensions, "deflate-stream", "extensions attribute deflate-stream");
      ws.close();
    }

    ws.onclose = function(e) {
      ok(true, "test 38 close");
      resolve();
    }
  });
}

// test39: a basic wss:// connectivity test
function test39() {
  return new Promise(function(resolve, reject) {
    var prots=["test-39"];

    var ws = CreateTestWS("wss://example.com/tests/dom/base/test/file_websocket", prots);
    status_test39 = "started";

    ws.onopen = function(e) {
      status_test39 = "opened";
      ok(true, "test 39 open");
      ws.close();
    }

    ws.onclose = function(e) {
      ok(true, "test 39 close");
      is(status_test39, "opened", "test 39 did open");
      resolve();
    }
  });
}

// test40: negative test for wss:// with no cert
function test40() {
  return new Promise(function(resolve, reject) {
    var prots=["test-40"];

    var ws = CreateTestWS("wss://nocert.example.com/tests/dom/base/test/file_websocket", prots);

    status_test40 = "started";
    ws.onerror = ignoreError;

    ws.onopen = function(e) {
      status_test40 = "opened";
      ok(false, "test 40 open");
      ws.close();
    }

    ws.onclose = function(e) {
      ok(true, "test 40 close");
      is(status_test40, "started", "test 40 did not open");
      resolve();
    }
  });
}

// test41: HSTS
function test41() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://example.com/tests/dom/base/test/file_websocket", "test-41a", 1);

    ws.onopen = function(e) {
      ok(true, "test 41a open");
      is(ws.url, "ws://example.com/tests/dom/base/test/file_websocket",
         "test 41a initial ws should not be redirected");
      ws.close();
    }

    ws.onclose = function(e) {
      ok(true, "test 41a close");

      // establish a hsts policy for example.com
      var wsb = CreateTestWS("wss://example.com/tests/dom/base/test/file_websocket", "test-41b", 1);

      wsb.onopen = function(e) {
        ok(true, "test 41b open");
        wsb.close();
      }

      wsb.onclose = function(e) {
        ok(true, "test 41b close");

        // try ws:// again, it should be done over wss:// now due to hsts
        var wsc = CreateTestWS("ws://example.com/tests/dom/base/test/file_websocket", "test-41c");

        wsc.onopen = function(e) {
          ok(true, "test 41c open");
          is(wsc.url, "wss://example.com/tests/dom/base/test/file_websocket",
             "test 41c ws should be redirected by hsts to wss");
          wsc.close();
        }

        wsc.onclose = function(e) {
          ok(true, "test 41c close");

          // clean up the STS state
          const Ci = SpecialPowers.Ci;
          var loadContext = SpecialPowers.wrap(window)
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsILoadContext);
          var flags = 0;
          if (loadContext.usePrivateBrowsing)
            flags |= Ci.nsISocketProvider.NO_PERMANENT_STORAGE;
          SpecialPowers.cleanUpSTSData("http://example.com", flags);
          resolve();
         }
       }
    }
  });
}

// test42: non-char utf-8 sequences
function test42() {
  return new Promise(function(resolve, reject) {
    // test some utf-8 non-characters. They should be allowed in the
    // websockets context. Test via round trip echo.
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-42");
    var data = ["U+FFFE \ufffe",
		"U+FFFF \uffff",
		"U+10FFFF \udbff\udfff"];
    var index = 0;

    ws.onopen = function() {
      ws.send(data[0]);
      ws.send(data[1]);
      ws.send(data[2]);
    }

    ws.onmessage = function(e) {
      is(e.data, data[index], "bad received message in test-42! index="+index);
      index++;
      if (index == 3) {
        ws.close();
      }
    }

    ws.onclose = function(e) {
      resolve();
    }
  });
}

// test43: Test setting binaryType attribute
function test43() {
  return new Promise(function(resolve, reject) {
    var prots=["test-43"];

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", prots);

    ws.onopen = function(e) {
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
    }

    ws.onclose = function(e) {
      ok(true, "test 43 close");
      resolve();
    }
  });
}

// test44: Test sending/receving binary ArrayBuffer
function test44() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-44");
    is(ws.readyState, 0, "bad readyState in test-44!");
    ws.binaryType = "arraybuffer";

    ws.onopen = function() {
      is(ws.readyState, 1, "open bad readyState in test-44!");
      var buf = new ArrayBuffer(3);
      // create byte view
      var view = new Uint8Array(buf);
      view[0] = 5;
      view[1] = 0; // null byte
      view[2] = 7;
      ws.send(buf);
    }

    ws.onmessage = function(e) {
      ok(e.data instanceof ArrayBuffer, "Should receive an arraybuffer!");
      var view = new Uint8Array(e.data);
      ok(view.length == 2 && view[0] == 0 && view[1] ==4, "testing Reply arraybuffer" );
      ws.close();
    }

    ws.onclose = function(e) {
      is(ws.readyState, 3, "onclose bad readyState in test-44!");
      shouldCloseCleanly(e);
      resolve();
    }
  });
}

// test45: Test sending/receving binary Blob
function test45() {
  return new Promise(function(resolve, reject) {
    function test45Real(blobFile) {
      var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-45");
      is(ws.readyState, 0, "bad readyState in test-45!");
      // ws.binaryType = "blob";  // Don't need to specify: blob is the default

      ws.onopen = function() {
        is(ws.readyState, 1, "open bad readyState in test-45!");
        ws.send(blobFile);
      }

      var test45blob;

      ws.onmessage = function(e) {
        test45blob = e.data;
        ok(test45blob instanceof Blob, "We should be receiving a Blob");

        ws.close();
      }

      ws.onclose = function(e) {
        is(ws.readyState, 3, "onclose bad readyState in test-45!");
        shouldCloseCleanly(e);

        // check blob contents
        var reader = new FileReader();
        reader.onload = function(event) {
          is(reader.result, "flob", "response should be 'flob': got '"
             + reader.result + "'");
        }

        reader.onerror = function(event) {
          testFailed("Failed to read blob: error code = " + reader.error.code);
        }

        reader.onloadend = function(event) {
          resolve();
        }

        reader.readAsBinaryString(test45blob);
      }
    }

    SpecialPowers.createFiles([{name: "testBlobFile", data: "flob"}],
    function(files) {
      test45Real(files[0]);
    },
    function(msg) {
      testFailed("Failed to create file for test45: " + msg);
      resolve();
    });
  });
}

// test46: Test that we don't dispatch incoming msgs once in CLOSING state
function test46() {
  return new Promise(function(resolve, reject) {
    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-46");
    is(ws.readyState, 0, "create bad readyState in test-46!");

    ws.onopen = function() {
      is(ws.readyState, 1, "open bad readyState in test-46!");
      ws.close()
      is(ws.readyState, 2, "close must set readyState to 2 in test-46!");
    }

    ws.onmessage = function(e) {
      ok(false, "received message after calling close in test-46!");
    }

    ws.onclose = function(e) {
      is(ws.readyState, 3, "onclose bad readyState in test-46!");
      shouldCloseCleanly(e);
      resolve();
    }
  });
}

// test47: Make sure onerror/onclose aren't called during close()
function test47() {
  return new Promise(function(resolve, reject) {
    var hasError = false;
    var ws = CreateTestWS("ws://another.websocket.server.that.probably.does.not.exist");

    ws.onopen = shouldNotOpen;

    ws.onerror = function (e) {
      is(ws.readyState, 3, "test-47: readyState should be CLOSED(3) in onerror: got "
         + ws.readyState);
      ok(!ws._withinClose, "onerror() called during close()!");
      hasError = true;
    }

    ws.onclose = function(e) {
      shouldCloseNotCleanly(e);
      ok(hasError, "test-47: should have called onerror before onclose");
      is(ws.readyState, 3, "test-47: readyState should be CLOSED(3) in onclose: got "
         + ws.readyState);
      ok(!ws._withinClose, "onclose() called during close()!");
      is(e.code, 1006, "test-47 close code should be 1006 but is:" + e.code);
      resolve();
    }

    // Call close before we're connected: throws error
    // Make sure we call onerror/onclose asynchronously
    ws._withinClose = 1;
    ws.close(3333, "Closed before we were open: error");
    ws._withinClose = 0;
    is(ws.readyState, 2, "test-47: readyState should be CLOSING(2) after close(): got "
       + ws.readyState);
  });
}

// test48: see bug 1227136 - client calls close() from onopen() and waits until
// WebSocketChannel::mSocketIn is nulled out on socket thread.
function test48() {
  return new Promise(function(resolve, reject) {
    const pref_close = "network.websocket.timeout.close";
    SpecialPowers.setIntPref(pref_close, 1);

    var ws = CreateTestWS("ws://mochi.test:8888/tests/dom/base/test/file_websocket", "test-48");

    ws.onopen = function() {
      ws.close();

      var date = new Date();
      var curDate = null;
      do {
        curDate = new Date();
      } while(curDate-date < 1500);

    }

    ws.onclose = function(e) {
      ok(true, "ws close in test 48");
      resolve();
    }

    SpecialPowers.clearUserPref(pref_close);
  });
}
