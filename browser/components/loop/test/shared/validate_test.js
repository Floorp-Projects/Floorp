/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*global chai, validate */

var expect = chai.expect;

describe("Validator", function() {
  "use strict";

  // test helpers
  function create(dependencies, values) {
    var validator = new loop.validate.Validator(dependencies);
    return validator.validate.bind(validator, values);
  }

  // test types
  function X(){}
  function Y(){}

  describe("#validate", function() {
    it("should check for a single required dependency when no option passed",
      function() {
        expect(create({x: Number}, {}))
          .to.Throw(TypeError, /missing required x$/);
      });

    it("should check for a missing required dependency, undefined passed",
      function() {
        expect(create({x: Number}, {x: undefined}))
          .to.Throw(TypeError, /missing required x$/);
      });

    it("should check for multiple missing required dependencies", function() {
      expect(create({x: Number, y: String}, {}))
        .to.Throw(TypeError, /missing required x, y$/);
    });

    it("should check for required dependency types", function() {
      expect(create({x: Number}, {x: "woops"})).to.Throw(
        TypeError, /invalid dependency: x; expected Number, got String$/);
    });

    it("should check for a dependency to match at least one of passed types",
      function() {
        expect(create({x: [X, Y]}, {x: 42})).to.Throw(
          TypeError, /invalid dependency: x; expected X, Y, got Number$/);
        expect(create({x: [X, Y]}, {x: new Y()})).to.not.Throw();
      });

    it("should skip type check if required dependency type is undefined",
      function() {
        expect(create({x: undefined}, {x: /whatever/})).not.to.Throw();
      });

    it("should check for a String dependency", function() {
      expect(create({foo: String}, {foo: 42})).to.Throw(
        TypeError, /invalid dependency: foo/);
    });

    it("should check for a Number dependency", function() {
      expect(create({foo: Number}, {foo: "x"})).to.Throw(
        TypeError, /invalid dependency: foo/);
    });

    it("should check for a custom constructor dependency", function() {
      expect(create({foo: X}, {foo: null})).to.Throw(
        TypeError, /invalid dependency: foo; expected X, got null$/);
    });

    it("should check for a native constructor dependency", function() {
      expect(create({foo: mozRTCSessionDescription}, {foo: "x"}))
        .to.Throw(TypeError,
                  /invalid dependency: foo; expected mozRTCSessionDescription/);
    });

    it("should check for a null dependency", function() {
      expect(create({foo: null}, {foo: "x"})).to.Throw(
        TypeError, /invalid dependency: foo; expected null, got String$/);
    });
  });
});
