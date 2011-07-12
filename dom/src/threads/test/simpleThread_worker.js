function messageListener(event) {
  switch (event.data) {
    case "no-op":
      break;
    case "components":
      postMessage(Components.toString());
      break;
    case "start":
      for (var i = 0; i < 1000; i++) { }
      postMessage("started");
      break;
    case "stop":
      for (var i = 0; i < 1000; i++) { }
      self.postMessage('no-op');
      postMessage("stopped");
      self.removeEventListener("message", messageListener, false);
      break;
    default:
      throw 'Bad message: ' + event.data;
  }
}

addEventListener("message", messageListener, false);
