// |reftest| 

// Validate CCWs and proxies
class Base {
  constructor(o) {
    return o;
  }
}

class A extends Base {
  x1 = 12;
  #x = 10;
  static gx(o) {
    return o.#x;
  }
  static sx(o, x) {
    o.#x = x;
  }
  static hasx(o) {
    try {
      o.#x;
      return true;
    } catch {
      return false;
    }
  }
}


var g = newGlobal({newCompartment: true});
g.A = A;

// cross_compartment_target is a cross compartment wrapper to an empty object.
var cross_compartment_target = g.eval('this.x = {}; this.x');

// #x gets stamped into the target of the CCW.
new A(cross_compartment_target);
assertEq(A.hasx(cross_compartment_target), true);

// Can we update and read from this compartment?
assertEq(A.gx(cross_compartment_target), 10);
var o = {test: 12};
A.sx(cross_compartment_target, o);
assertEq(A.gx(cross_compartment_target), o);

// Can we read and update from the other compartment?
assertEq(g.eval('this.A.gx(this.x)'), o);
var y = g.eval('this.y = {test: 13}; this.A.sx(this.x, this.y); this.y');
assertEq(g.eval('this.A.gx(this.x)'), y);
assertEq(A.gx(cross_compartment_target), y);


if (typeof nukeCCW === 'function') {
  // Nuke the CCW. Now things should throw.
  nukeCCW(cross_compartment_target);
  var threw = true;
  try {
    A.gx(cross_compartment_target);
    threw = false;
  } catch (e) {
  }
  assertEq(threw, true);
}


if (typeof reportCompare === 'function') reportCompare(0, 0);