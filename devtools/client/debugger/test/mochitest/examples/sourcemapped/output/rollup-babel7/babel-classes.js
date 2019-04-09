var rollupBabel7BabelClasses = (function () {
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

  function _defineProperty(obj, key, value) {
    if (key in obj) {
      Object.defineProperty(obj, key, {
        value: value,
        enumerable: true,
        configurable: true,
        writable: true
      });
    } else {
      obj[key] = value;
    }

    return obj;
  }

  function root() {
    var Another =
    /*#__PURE__*/
    function () {
      function Another() {
        var _this = this;

        _classCallCheck(this, Another);

        _defineProperty(this, "bound", function () {
          return _this;
        });
      }

      _createClass(Another, [{
        key: "method",
        value: function method() {
          console.log("pause here", Another, root);
        }
      }]);

      return Another;
    }();

    new Another().method();
  }

  return root;

}());
//# sourceMappingURL=babel-classes.js.map
