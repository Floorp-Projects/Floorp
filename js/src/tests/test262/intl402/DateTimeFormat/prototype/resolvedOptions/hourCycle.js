// Copyright 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.resolvedOptions
description: >
  Intl.DateTimeFormat.prototype.resolvedOptions properly
  reflect hourCycle settings.
info: >
  12.4.5 Intl.DateTimeFormat.prototype.resolvedOptions()

includes: [testIntl.js]
---*/

/* Values passed via unicode extension key work */

/**
 * Since at the moment of writing, CLDR does not provide data for any locale
 * that would allow it to use both 0-based and 1-based hourCycles,
 * we can only test if the result is within the pair of h11/h12 or h23/h24.
 */
const hcValuePairs = [
  ["h11", "h12"],
  ["h23", "h24"]
];

const hour12Values = ['h11', 'h12'];
const hour24Values = ['h23', 'h24'];

for (const hcValuePair of hcValuePairs) {
  for (const hcValue of hcValuePair) {
    const resolvedOptions = new Intl.DateTimeFormat(`de-u-hc-${hcValue}`, {
      hour: 'numeric'
    }).resolvedOptions();

    mustHaveProperty(resolvedOptions, 'hourCycle', hcValuePair);
    mustHaveProperty(resolvedOptions, 'hour12', [hour12Values.includes(hcValue)]);
  }
}

/* Values passed via options work */

for (const hcValuePair of hcValuePairs) {
  for (const hcValue of hcValuePair) {
    const resolvedOptions = new Intl.DateTimeFormat(`en-US`, {
      hour: 'numeric',
      hourCycle: hcValue
    }).resolvedOptions();

    mustHaveProperty(resolvedOptions, 'hourCycle', hcValuePair);
    mustHaveProperty(resolvedOptions, 'hour12', [hour12Values.includes(hcValue)]);
  }
}

/* When both extension key and option is passed, option takes precedence */

let resolvedOptions = new Intl.DateTimeFormat(`en-US-u-hc-h12`, {
  hour: 'numeric',
  hourCycle: 'h23'
}).resolvedOptions();

mustHaveProperty(resolvedOptions, 'hourCycle', ['h23', 'h24']);
mustHaveProperty(resolvedOptions, 'hour12', [false]);

/* When hour12 and hourCycle are set, hour12 takes precedence */

resolvedOptions = new Intl.DateTimeFormat(`fr`, {
  hour: 'numeric',
  hour12: true,
  hourCycle: 'h23'
}).resolvedOptions();

mustHaveProperty(resolvedOptions, 'hourCycle', ['h11', 'h12']);
mustHaveProperty(resolvedOptions, 'hour12', [true]);

/* When hour12 and extension key are set, hour12 takes precedence */

resolvedOptions = new Intl.DateTimeFormat(`fr-u-hc-h24`, {
  hour: 'numeric',
  hour12: true,
}).resolvedOptions();

mustHaveProperty(resolvedOptions, 'hourCycle', ['h11', 'h12']);
mustHaveProperty(resolvedOptions, 'hour12', [true]);

reportCompare(0, 0);
