onerror = function(event) {
  // Do nothing.
};

// Pure JS recursion
function recurse() {
  recurse();
}

// JS -> C++ -> JS -> C++ recursion
function recurse2() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    xhr.open("GET", "nonexistent.file");
  }
  xhr.open("GET", "nonexistent.file");
  postMessage("Done");
}

var count = 0;
onmessage = function(event) {
  switch (++count) {
    case 1:
      recurse();
      break;
    case 2:
      recurse2();
      // Have to return here because we don't propagate exceptions from event
      // handlers!
      return;
    default:
  }
  throw "Never should have gotten here!";
}
