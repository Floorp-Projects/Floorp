// |reftest| skip-if(!this.hasOwnProperty("Temporal"))

const ArrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());

// Modify the ArrayIteratorPrototype prototype chain to disable optimisations.
Object.setPrototypeOf(ArrayIteratorPrototype, {});

let calendar = new Temporal.Calendar("iso8601");

let dateLike = {
  calendar,
  day: 1,
  month: 1,
  year: 0,
};

let result = Temporal.PlainDate.from(dateLike);

assertEq(result.toString(), "0000-01-01");

if (typeof reportCompare === "function")
  reportCompare(true, true);
