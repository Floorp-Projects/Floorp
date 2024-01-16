/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This function should never run on a too much recursion error.
onerror = function (event) {
  postMessage(event.message);
};

// Pure JS recursion
function recurse() {
  recurse();
}

// JS -> C++ -> JS -> C++ recursion
function recurse2() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    xhr.open("GET", "nonexistent.file");
  };
  xhr.open("GET", "nonexistent.file");
}

var messageCount = 0;
onmessage = function (event) {
  switch (++messageCount) {
    case 2:
      recurse2();

      // An exception thrown from an event handler like xhr.onreadystatechange
      // should not leave an exception pending in the code that generated the
      // event.
      postMessage("Done");
      return;

    case 1:
      recurse();
      throw "Exception should have prevented us from getting here!";

    default:
      throw "Weird number of messages: " + messageCount;
  }

  // eslint-disable-next-line no-unreachable
  throw "Impossible to get here!";
};
