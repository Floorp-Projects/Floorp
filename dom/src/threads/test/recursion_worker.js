// Pure JS recursion
function recurse() {
  recurse();
}

// JS -> C++ -> JS -> C++ recursion
function recurse2() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    xhr.abort();
    xhr.open("GET", "nonexistent.file");
  }
  xhr.open("GET", "nonexistent.file");
}

var count = 0;
onmessage = function(event) {
  switch (++count) {
    case 1:
      recurse();
      break;
    case 2:
      recurse2();
      break;
    default:
  }
  throw "Never should have gotten here!";
}
