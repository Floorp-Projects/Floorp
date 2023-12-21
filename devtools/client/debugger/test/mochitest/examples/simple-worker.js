var count = 0;
function timer() {
  var n = ++count;
  console.log("WORKER SAYS HELLO! " + n);
}

setInterval(timer, 1000);

self.onmessage = function onmessage() {
  console.log("hi");
};
