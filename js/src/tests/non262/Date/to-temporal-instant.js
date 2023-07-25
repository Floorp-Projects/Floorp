// |reftest| skip-if(!this.hasOwnProperty("Temporal"))

const min = new Date(-8640000000000000).toTemporalInstant();
const max = new Date(8640000000000000).toTemporalInstant();
const epoch = new Date(0).toTemporalInstant();

const minTemporalInstant = new Temporal.Instant(-8640000000000000000000n)
const maxTemporalInstant = new Temporal.Instant(8640000000000000000000n)
const zeroInstant = new Temporal.Instant(0n)

let zero = Temporal.Duration.from({nanoseconds: 0});
let one = Temporal.Duration.from({nanoseconds: 1});
let minusOne = Temporal.Duration.from({nanoseconds: -1});

//Test invalid date
{
    const invalidDate = new Date(NaN);
    assertThrowsInstanceOf(() => invalidDate.toTemporalInstant(), RangeError);
}

//Test Temporal.Instant properties
{
    // epochNanoseconds
    assertEq(min.epochNanoseconds, minTemporalInstant.epochNanoseconds);
    assertEq(max.epochNanoseconds, maxTemporalInstant.epochNanoseconds);
    assertEq(epoch.epochNanoseconds, zeroInstant.epochNanoseconds);

    // toZonedDateTime
    assertEq(min.toZonedDateTimeISO('UTC').toString(), minTemporalInstant.toZonedDateTimeISO('UTC').toString());
    assertEq(max.toZonedDateTimeISO('UTC').toString(), maxTemporalInstant.toZonedDateTimeISO('UTC').toString());
    assertEq(epoch.toZonedDateTimeISO('UTC').toString(), zeroInstant.toZonedDateTimeISO('UTC').toString());
}

// Test values around the minimum/maximum instant.
{
    // Adding zero to the minimum instant.
    assertEq(min.add(zero).epochNanoseconds, min.epochNanoseconds);
    assertEq(min.subtract(zero).epochNanoseconds, min.epochNanoseconds);

    // Adding zero to the maximum instant.
    assertEq(max.add(zero).epochNanoseconds, max.epochNanoseconds);
    assertEq(max.subtract(zero).epochNanoseconds, max.epochNanoseconds);

    // Adding one to the minimum instant.
    assertEq(min.add(one).epochNanoseconds, min.epochNanoseconds + 1n);
    assertEq(min.subtract(minusOne).epochNanoseconds, min.epochNanoseconds + 1n);

    // Subtracting one from the maximum instant.
    assertEq(max.add(minusOne).epochNanoseconds, max.epochNanoseconds - 1n);
    assertEq(max.subtract(one).epochNanoseconds, max.epochNanoseconds - 1n);

    // Subtracting one from the minimum instant.
    assertThrowsInstanceOf(() => min.add(minusOne), RangeError);
    assertThrowsInstanceOf(() => min.subtract(one), RangeError);

    // Adding one to the maximum instant.
    assertThrowsInstanceOf(() => max.add(one), RangeError);
    assertThrowsInstanceOf(() => max.subtract(minusOne), RangeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
