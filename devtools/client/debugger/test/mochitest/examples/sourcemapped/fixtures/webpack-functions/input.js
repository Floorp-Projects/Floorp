var module = {};

module.exports = function(x) {
  return x * 2;
};


export default function root() {
  // This example is structures to look like CommonJS in order to replicate
  // a previously-encountered bug.
  module.exports(4);
}
