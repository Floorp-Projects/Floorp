const times2 = require("./times2");
const { output } = require("./output");
const opts = require("./opts");

output(times2(1));
output(times2(2));

if(opts.extra) {
  output(times2(3));
}

window.keepMeAlive = function() {
  // This function exists to make sure this script is never garbage
  // collected. It is also callable from tests.
  return times2(4);
}
