function postMsg(message)
{
  parent.postMessage(message, "http://mochi.test:8888");
}

window.addEventListener("message", onMessageReceived, false);

function onMessageReceived(event)
{
  if (event.data == "step") {
    var performed = false;
    try {
      performed = doStep();
    }
    catch (ex) {
      postMsg("FAILURE: exception threw at "+ location +":\n" + ex);
      finishTest();
    }

    if (performed)
      postMsg("perf");

    return;
  }

  postMsg("Invalid message");
}

function ok(a, message)
{
  if (!a)
    postMsg("FAILURE: " + message);
  else
    postMsg(message);
}

function is(a, b, message)
{
  if (a != b)
    postMsg("FAILURE: " + message + ", expected "+b+" got "+a);
  else
    postMsg(message + ", expected "+b+" got "+a);
}

function todo(a, b, message)
{
  postMsg("TODO: " + message + ", expected "+b+" got "+a);
}

function finishTest()
{
  localStorage.clear();
  postMsg("done");
  return false;
}
