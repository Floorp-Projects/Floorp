var rollupBabel6Classes = (function () {
  'use strict';

  var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

  function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

  function root() {
    var one = 1;

    var Thing = function () {
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

    var Another = function () {
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
