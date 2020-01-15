var w = new Worker("inner-worker.js");

self.addEventListener(
  "message",
  function(e) {
    console.log("Outer worker received message!");
  }
);
