function postMsg(message)
{
  opener.postMessage(message, "http://mochi.test:8888");
}

window.addEventListener("message", onMessageReceived, false);

function onMessageReceived(event)
{
  //alert("slave got event: "+event.data);
  if (event.data == "step") {
    if (doStep())
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

function finishTest()
{
  sessionStorage.clear();
  postMsg("done");
  return false;
}
