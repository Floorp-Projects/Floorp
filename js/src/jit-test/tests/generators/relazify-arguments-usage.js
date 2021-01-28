
function * f() {
  yield arguments;
  yield arguments;
  yield arguments;
}

for (x of f()) {
  relazifyFunctions();
}
