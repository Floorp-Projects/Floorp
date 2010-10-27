function main() {
  try {
    throw "something";
  } catch(e) {
    return "failure";
  }
  return "unset";
}

setDebug(true);
setThrowHook("'success'");

assertEq(main(), "success");
