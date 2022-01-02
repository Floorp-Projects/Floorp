console.log("Hello from the inner worker!");

self.addEventListener(
  "message",
  function(e) {
    console.log("Inner worker received message!");
  }
);
