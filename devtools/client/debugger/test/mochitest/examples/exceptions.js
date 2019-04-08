function uncaughtException() {
  throw "unreachable"
}

function caughtError() {
  try {
    throw new Error("error");
  } catch (e) {
    debugger;
  }
}

function caughtException() {
  try {
    throw "reachable";
  } catch (e) {
    debugger;
  }
}
