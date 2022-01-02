let parentLocation = "";

// The first time this gets called in a page, the location of the parent
// should be passed in. This will be used as the target origin argument
// for the postMessage call for all subsequent calls to postMsg().
function postMsg(message, newParentLocation) {
  if (newParentLocation) {
    parentLocation = newParentLocation;
  } else if (parentLocation == "") {
    throw new Error("Failed to pass in newParentLocation");
  }

  parent.postMessage(message, parentLocation);
}

window.addEventListener("message", onMessageReceived);

function onMessageReceived(event) {
  if (event.data == "step") {
    var performed = false;
    try {
      performed = doStep();
    } catch (ex) {
      postMsg("FAILURE: exception threw at " + location + ":\n" + ex);
      finishTest();
    }

    if (performed) {
      postMsg("perf");
    }

    return;
  }

  if (parent) {
    postMsg(event.data);
  }
}

function ok(a, message) {
  if (!a) {
    postMsg("FAILURE: " + message);
  } else {
    postMsg(message);
  }
}

function is(a, b, message) {
  if (a != b) {
    postMsg("FAILURE: " + message + ", expected " + b + " got " + a);
  } else {
    postMsg(message + ", expected " + b + " got " + a);
  }
}

function todo(a, b, message) {
  postMsg("TODO: " + message + ", expected " + b + " got " + a);
}

function finishTest() {
  postMsg("done");
  return false;
}
