const times2 = require("./times2");
const { output } = require("./output");
const opts = require("./opts");

output(times2(1));
output(times2(2));

if(opts.extra) {
  output(times2(3));
}
