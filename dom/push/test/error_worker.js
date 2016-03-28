this.onpush = function(event) {
  var request = event.data.json();
  if (request.type == "exception") {
    throw new Error("Uncaught exception");
  }
  if (request.type == "rejection") {
    event.waitUntil(Promise.reject(
      new Error("Unhandled rejection")));
  }
};
