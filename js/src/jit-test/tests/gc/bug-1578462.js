// |jit-test| skip-if: !hasFunction["gczeal"]

// Check that incoming CCW edges are marked gray in a zone GC if the
// source is gray.

// Set up a new compartment with a gray object wrapper to this
// compartment.
function createOtherCompartment() {
    let t = {};
    addMarkObservers([t]);
    let g = newGlobal({newCompartment: true});
    g.t = t;
    g.eval(`grayRoot().push(t);`);
    g.t = null;
    t = null;
    return g;
}

function startGCMarking() {
  startgc(1);
  while (gcstate() === "Prepare") {
    gcslice(1);
  }
}

gczeal(0);

let g = createOtherCompartment();

// The target should be gray in a full GC...
gc();
assertEq(getMarks()[0], "gray");

// and subsequently gray in a zone GC of only this compartment.
gc(this);
assertEq(getMarks()[0], "gray");

// If a barrier marks the gray wrapper black after the start of the
// GC, the target ends up black.
schedulezone(this);
startGCMarking()
assertEq(getMarks()[0], "unmarked");
g.eval(`grayRoot()`); // Barrier marks gray roots black.
assertEq(getMarks()[0], "black");
finishgc();

// A full collection resets the wrapper to gray.
gc();
assertEq(getMarks()[0], "gray");

// If a barrier marks the gray wrapper black after the target has
// already been marked gray, the target ends up black.
gczeal(25); // Yield during gray marking.
schedulezone(this);
startGCMarking();
assertEq(getMarks()[0], "gray");
g.eval(`grayRoot()`); // Barrier marks gray roots black.
assertEq(getMarks()[0], "black");
finishgc();
