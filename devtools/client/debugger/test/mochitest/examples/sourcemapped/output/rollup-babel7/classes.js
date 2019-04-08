var rollupBabel7Classes = (function () {
  'use strict';

  function _classCallCheck(instance, Constructor) {
    if (!(instance instanceof Constructor)) {
      throw new TypeError("Cannot call a class as a function");
    }
  }

  function _defineProperties(target, props) {
    for (var i = 0; i < props.length; i++) {
      var descriptor = props[i];
      descriptor.enumerable = descriptor.enumerable || false;
      descriptor.configurable = true;
      if ("value" in descriptor) descriptor.writable = true;
      Object.defineProperty(target, descriptor.key, descriptor);
    }
  }

  function _createClass(Constructor, protoProps, staticProps) {
    if (protoProps) _defineProperties(Constructor.prototype, protoProps);
    if (staticProps) _defineProperties(Constructor, staticProps);
    return Constructor;
  }

  function root() {
    var one = 1;

    var Thing =
    /*#__PURE__*/
    function () {
      function Thing() {
        _classCallCheck(this, Thing);
      }

      _createClass(Thing, [{
        key: "one",
        value: function one() {
          console.log("pause here");
        }
      }]);

      return Thing;
    }();

    var Another =
    /*#__PURE__*/
    function () {
      function Another() {
        _classCallCheck(this, Another);
      }

      _createClass(Another, [{
        key: "method",
        value: function method() {
          console.log("pause here", Another, one, Thing, root);
        }
      }]);

      return Another;
    }();

    new Another().method();
    new Thing().one();
  }

  return root;

}());
//# sourceMappingURL=classes.js.map
