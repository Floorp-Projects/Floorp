var rollupBabel6BabelClasses = (function () {
  'use strict';

  var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

  function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

  function root() {
    var Another = function () {
      function Another() {
        var _this = this;

        _classCallCheck(this, Another);

        this.bound = function () {
          return _this;
        };
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
