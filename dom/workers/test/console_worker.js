/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

onmessage = function(event) {
  // TEST: does console exist?
  postMessage({event: 'console exists', status: !!console, last : false});

  postMessage({event: 'console is the same object', status: console === console, last: false});

  postMessage({event: 'trace without function', status: true, last : false});

  for (var i = 0; i < 10; ++i) {
    console.log(i, i, i);
  }

  function trace1() {
    function trace2() {
      function trace3() {
        console.trace("trace " + i);
      }
      trace3();
    }
    trace2();
  }
  trace1();

  foobar585956c = function(a) {
    console.trace();
    return a+"c";
  };

  function foobar585956b(a) {
    return foobar585956c(a+"b");
  }

  function foobar585956a(omg) {
    return foobar585956b(omg + "a");
  }

  function foobar646025(omg) {
    console.log(omg, "o", "d");
  }

  function startTimer(timer) {
    console.time(timer);
  }

  function stopTimer(timer) {
    console.timeEnd(timer);
  }

  function timeStamp(label) {
    console.timeStamp(label);
  }

  function testGroups() {
    console.groupCollapsed("a", "group");
    console.group("b", "group");
    console.groupEnd("b", "group");
  }

  foobar585956a('omg');
  foobar646025('omg');
  timeStamp();
  timeStamp('foo');
  testGroups();
  startTimer('foo');
  setTimeout(function() {
    stopTimer('foo');
    nextSteps(event);
  }, 10);
}

function nextSteps(event) {

  function namelessTimer() {
    console.time();
    console.timeEnd();
  }

  namelessTimer();

  var str = "Test Message."
  console.log(str);
  console.info(str);
  console.warn(str);
  console.error(str);
  console.exception(str);
  console.assert(true, str);
  console.assert(false, str);
  console.profile(str);
  console.profileEnd(str);
  console.timeStamp();
  console.clear();
  postMessage({event: '4 messages', status: true, last : false});

  // Recursive:
  if (event.data == true) {
    var worker = new Worker('console_worker.js');
    worker.onmessage = function(event) {
      postMessage(event.data);
    }
    worker.postMessage(false);
  } else {
    postMessage({event: 'bye bye', status: true, last : true});
  }
}
