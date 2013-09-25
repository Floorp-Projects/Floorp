function postMsg(message)
{
  var l = SpecialPowers.wrap(parent.window.location);
  parent.postMessage(message, l.protocol + "//" + l.host);
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

  if (parent)
    postMsg(event.data);
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
  postMsg("done");
  return false;
}
