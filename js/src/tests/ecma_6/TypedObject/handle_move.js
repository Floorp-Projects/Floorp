// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 898342;
var summary = 'Handle Move';

var T = TypedObject;

var Point = T.float32.array(3);
var Line = new T.StructType({from: Point, to: Point});
var Lines = Line.array(3);

function runTests() {
  function testHandleToPoint() {
    var lines = new Lines([
      {from: [1, 2, 3], to: [4, 5, 6]},
      {from: [7, 8, 9], to: [10, 11, 12]},
      {from: [13, 14, 15], to: [16, 17, 18]}
    ]);

    function allPoints(lines, func) {
      var handle = Point.handle();
      for (var i = 0; i < lines.length; i++) {
        T.Handle.move(handle, lines, i, "from");
        func(handle);

        T.Handle.move(handle, lines, i, "to");
        func(handle);
      }
    }

    // Iterate over all ponts and mutate them in place:
    allPoints(lines, function(p) {
      p[0] += 100;
      p[1] += 200;
      p[2] += 300;
    });

    // Spot check the results
    assertEq(lines[0].from[0], 101);
    assertEq(lines[1].to[1], 211);
    assertEq(lines[2].to[2], 318);
  }
  testHandleToPoint();

  function testHandleToFloat() {
    var lines = new Lines([
      {from: [1, 2, 3], to: [4, 5, 6]},
      {from: [7, 8, 9], to: [10, 11, 12]},
      {from: [13, 14, 15], to: [16, 17, 18]}
    ]);

    function allPoints(lines, func) {
      var handle = T.float32.handle();
      for (var i = 0; i < lines.length; i++) {
        T.Handle.move(handle, lines, i, "from", 0);
        func(handle);

        T.Handle.move(handle, lines, i, "from", 1);
        func(handle);

        T.Handle.move(handle, lines, i, "from", 2);
        func(handle);

        T.Handle.move(handle, lines, i, "to", 0);
        func(handle);

        T.Handle.move(handle, lines, i, "to", 1);
        func(handle);

        T.Handle.move(handle, lines, i, "to", 2);
        func(handle);
      }
    }

    // Iterate over all ponts and mutate them in place:
    allPoints(lines, function(p) {
      T.Handle.set(p, T.Handle.get(p) + 100);
    });

    // Spot check the results
    assertEq(lines[0].from[0], 101);
    assertEq(lines[1].to[1], 111);
    assertEq(lines[2].to[2], 118);
  }
  testHandleToFloat();

  function testHandleToEquivalentType() {
    var Point2 = T.float32.array(3);

    assertEq(Point.equivalent(Point2), true);

    var lines = new Lines([
      {from: [1, 2, 3], to: [4, 5, 6]},
      {from: [7, 8, 9], to: [10, 11, 12]},
      {from: [13, 14, 15], to: [16, 17, 18]}
    ]);

    var handle = Point2.handle(lines, 0, "to");
    assertEq(handle[0], 4);
    assertEq(handle[1], 5);
    assertEq(handle[2], 6);
  }
  testHandleToEquivalentType();

  function testHandleMoveToIllegalType() {
    var lines = new Lines([
      {from: [1, 2, 3], to: [4, 5, 6]},
      {from: [7, 8, 9], to: [10, 11, 12]},
      {from: [13, 14, 15], to: [16, 17, 18]}
    ]);

    // Moving a handle to a value of incorrect type should report an error:
    assertThrowsInstanceOf(function() {
      Line.handle(lines);
    }, TypeError, "handle moved to destination of incorrect type");
    assertThrowsInstanceOf(function() {
      var h = Line.handle();
      T.Handle.move(h, lines);
    }, TypeError, "handle moved to destination of incorrect type");
    assertThrowsInstanceOf(function() {
      var h = T.float32.handle();
      T.Handle.move(h, lines, 0);
    }, TypeError, "handle moved to destination of incorrect type");
  }
  testHandleMoveToIllegalType();

  function testHandleMoveToIllegalProperty() {
    var lines = new Lines([
      {from: [1, 2, 3], to: [4, 5, 6]},
      {from: [7, 8, 9], to: [10, 11, 12]},
      {from: [13, 14, 15], to: [16, 17, 18]}
    ]);

    assertThrowsInstanceOf(function() {
      var h = Point.handle();
      T.Handle.move(h, lines, 0, "foo");
    }, TypeError, "No such property: foo");

    assertThrowsInstanceOf(function() {
      var h = Point.handle();
      T.Handle.move(h, lines, 22, "to");
    }, TypeError, "No such property: 22");

    assertThrowsInstanceOf(function() {
      var h = Point.handle();
      T.Handle.move(h, lines, -100, "to");
    }, TypeError, "No such property: -100");
  }
  testHandleMoveToIllegalProperty();

  reportCompare(true, true);
  print("Tests complete");
}

runTests();


