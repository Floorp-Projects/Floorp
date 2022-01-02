function named(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = root;
function root() {
  function fn(arg) {
    var _this = this,
        _arguments = arguments;

    console.log(this, arguments);
    console.log("pause here", fn, root);

    var arrow = function arrow(argArrow) {
      console.log(_this, _arguments);
      console.log("pause here", fn, root);
    };
    arrow("arrow-arg");
  }

  fn.call("this-value", "arg-value");
}
module.exports = exports["default"];

}
