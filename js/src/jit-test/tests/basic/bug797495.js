try {
  // Don't assert.
  options("strict_mode");
  var f = Function("for(w in\\");
} catch (e) {
}
