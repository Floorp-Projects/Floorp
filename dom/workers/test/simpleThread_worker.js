/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
function messageListener(event) {
  var exception;
  try {
    event.bubbles = true;
  }
  catch(e) {
    exception = e;
  }

  if (!(exception instanceof TypeError) ||
      exception.message != "setting a property that has only a getter") {
    throw exception;
  }

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
      self.postMessage('no-op');
      postMessage("stopped");
      self.removeEventListener("message", messageListener, false);
      break;
    default:
      throw 'Bad message: ' + event.data;
  }
}

addEventListener("message", { handleEvent: messageListener });
