// Test failure when constructing a function whose |.prototype| property isn't an object.

function Klass() {
  this.prop = 1;
}

// Save the original prototype.
const KlassPrototype = Klass.prototype;

// Set the prototype to a non-object value.
Klass.prototype = null;

const prototypes = [
  null,
  KlassPrototype,
];

const N = 500;
let c = 0;

for (let i = 0; i <= N; ++i) {
  // Always perform a set to avoid a cold-code bailout.
  let proto = prototypes[(i === N)|0];
  Klass.prototype = proto;

  // Create a new object.
  let o = new Klass();

  // Read a property from the new object to ensure it was correctly allocated
  // and initialised.
  c += o.prop;

  // The prototype defaults to %Object.prototype% when the |.prototype|
  // property isn't an object.
  assertEq(Object.getPrototypeOf(o), proto === null ? Object.prototype : KlassPrototype);
}

assertEq(c, N + 1);
