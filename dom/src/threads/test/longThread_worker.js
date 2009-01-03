onmessage = function(event) {
  switch (event.data) {
    case "start":
      for (var i = 0; i < 10000000; i++) { };
      postMessage("done");
      break;
    default:
      throw "Bad message: " + event.data;
  }
};
