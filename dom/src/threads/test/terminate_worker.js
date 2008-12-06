onmessage = function(event) {
  throw "No messages should reach me!";
}

setInterval(function() { postMessage("Still alive!"); }, 100);
