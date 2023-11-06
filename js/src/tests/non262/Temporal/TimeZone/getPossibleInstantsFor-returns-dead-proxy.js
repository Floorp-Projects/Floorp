// |reftest| skip-if(!this.hasOwnProperty("Temporal")||!xulRuntime.shell)

var g = newGlobal({newCompartment: true});

var tz = new class extends Temporal.TimeZone {
  *getPossibleInstantsFor() {
    // Return an Instant from a different compartment.
    yield new g.Temporal.Instant(0n);

    // Turn the CCW into a dead proxy wrapper.
    nukeAllCCWs();
  }
}("UTC");

var dt = new Temporal.PlainDateTime(2000, 1, 1);

// Throws a TypeError when attempting to unwrap the dead proxy.
assertThrowsInstanceOf(() => dt.toZonedDateTime(tz), TypeError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
