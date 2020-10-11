// |reftest| skip-if(!this.hasOwnProperty('Intl'))

// BCP47 since forever, and ECMA-402 as consequence, do not consider tags that
// contain duplicate variants to be structurally valid.

function mustReject(tag) {
  assertThrowsInstanceOf(() => {
    // Direct matches are rejected.
    new Intl.Locale(tag);
  }, RangeError, `tag "${tag}" must be considered structurally invalid`);
}

// Direct matches are rejected.
mustReject("en-emodeng-emodeng");
// Case-insensitive matches are also rejected.
mustReject("en-Emodeng-emodeng");
// ...and in either order.
mustReject("en-emodeng-Emodeng");

// Repeat the above tests with additional variants interspersed at each point
// for completeness.
mustReject("en-variant-emodeng-emodeng");
mustReject("en-variant-Emodeng-emodeng");
mustReject("en-variant-emodeng-Emodeng");
mustReject("en-emodeng-variant-emodeng");
mustReject("en-Emodeng-variant-emodeng");
mustReject("en-emodeng-variant-Emodeng");
mustReject("en-emodeng-emodeng-variant");
mustReject("en-Emodeng-emodeng-variant");
mustReject("en-emodeng-Emodeng-variant");

// This same restriction should also be applied to the |tlang| component --
// indicating the source locale from which relevant content was transformed --
// of a broader language tag.
//
// ECMA-402 doesn't yet standardize this, hence the non262 test.  But it's
// expected to do so soon.  See <https://github.com/tc39/ecma402/pull/429>.

// Direct matches are rejected.
mustReject("de-t-en-emodeng-emodeng");
// Case-insensitive matches are also rejected.
mustReject("de-t-en-Emodeng-emodeng");
// ...and in either order.
mustReject("de-t-en-emodeng-Emodeng");

// And again, repeat with additional variants interspersed at each point.
mustReject("de-t-en-variant-emodeng-emodeng");
mustReject("de-t-en-variant-Emodeng-emodeng");
mustReject("de-t-en-variant-emodeng-Emodeng");
mustReject("de-t-en-emodeng-variant-emodeng");
mustReject("de-t-en-Emodeng-variant-emodeng");
mustReject("de-t-en-emodeng-variant-Emodeng");
mustReject("de-t-en-emodeng-emodeng-variant");
mustReject("de-t-en-Emodeng-emodeng-variant");
mustReject("de-t-en-emodeng-Emodeng-variant");

if (typeof reportCompare === "function")
  reportCompare(true, true);
