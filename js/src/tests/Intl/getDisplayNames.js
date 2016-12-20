// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras'))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the getCalendarInfo function with a diverse set of arguments.

/*
 * Test if getDisplayNames return value matches expected values.
 */
function checkDisplayNames(names, expected)
{
  assertEq(Object.getPrototypeOf(names), Object.prototype);

  assertEq(names.locale, expected.locale);
  assertEq(names.style, expected.style);

  const nameValues = names.values;
  const expectedValues = expected.values;

  const nameValuesKeys = Object.getOwnPropertyNames(nameValues).sort();
  const expectedValuesKeys = Object.getOwnPropertyNames(expectedValues).sort();

  assertEqArray(nameValuesKeys, expectedValuesKeys);

  for (let key of expectedValuesKeys)
    assertEq(nameValues[key], expectedValues[key]);
}

addIntlExtras(Intl);

let gDN = Intl.getDisplayNames;

assertEq(gDN.length, 2);

checkDisplayNames(gDN('en-US', {
}), {
  locale: 'en-US',
  style: 'long',
  values: {}
});

checkDisplayNames(gDN('en-US', {
  keys: [
    'dates/gregorian/weekdays/wednesday'
  ],
  style: 'narrow'
}), {
  locale: 'en-US',
  style: 'narrow',
  values: {
    'dates/gregorian/weekdays/wednesday': 'W'
  }
});

checkDisplayNames(gDN('en-US', {
  keys: [
    'dates/fields/year',
    'dates/fields/month',
    'dates/fields/week',
    'dates/fields/day',
    'dates/gregorian/months/january',
    'dates/gregorian/months/february',
    'dates/gregorian/months/march',
    'dates/gregorian/weekdays/tuesday'
  ]
}), {
  locale: 'en-US',
  style: 'long',
  values: {
    'dates/fields/year': 'year',
    'dates/fields/month': 'month',
    'dates/fields/week': 'week',
    'dates/fields/day': 'day',
    'dates/gregorian/months/january': 'January',
    'dates/gregorian/months/february': 'February',
    'dates/gregorian/months/march': 'March',
    'dates/gregorian/weekdays/tuesday': 'Tuesday',
  }
});

checkDisplayNames(gDN('fr', {
  keys: [
    'dates/fields/year',
    'dates/fields/day',
    'dates/gregorian/months/october',
    'dates/gregorian/weekdays/saturday',
    'dates/gregorian/dayperiods/pm'
  ]
}), {
  locale: 'fr',
  style: 'long',
  values: {
    'dates/fields/year': 'année',
    'dates/fields/day': 'jour',
    'dates/gregorian/months/october': 'octobre',
    'dates/gregorian/weekdays/saturday': 'samedi',
    'dates/gregorian/dayperiods/pm': 'PM'
  }
});

checkDisplayNames(gDN('it', {
  style: 'short',
  keys: [
    'dates/gregorian/weekdays/thursday',
    'dates/gregorian/months/august',
    'dates/gregorian/dayperiods/am',
    'dates/fields/month',
  ]
}), {
  locale: 'it',
  style: 'short',
  values: {
    'dates/gregorian/weekdays/thursday': 'gio',
    'dates/gregorian/months/august': 'ago',
    'dates/gregorian/dayperiods/am': 'AM',
    'dates/fields/month': 'mese'
  }
});

checkDisplayNames(gDN('ar', {
  style: 'long',
  keys: [
    'dates/gregorian/weekdays/thursday',
    'dates/gregorian/months/august',
    'dates/gregorian/dayperiods/am',
    'dates/fields/month',
  ]
}), {
  locale: 'ar',
  style: 'long',
  values: {
    'dates/gregorian/weekdays/thursday': 'الخميس',
    'dates/gregorian/months/august': 'أغسطس',
    'dates/gregorian/dayperiods/am': 'ص',
    'dates/fields/month': 'الشهر'
  }
});

/* Invalid input */

assertThrowsInstanceOf(() => {
  gDN('en-US', {
    style: '',
    keys: [
      'dates/gregorian/weekdays/thursday',
    ]
  });
}, RangeError);

assertThrowsInstanceOf(() => {
  gDN('en-US', {
    style: 'bogus',
    keys: [
      'dates/gregorian/weekdays/thursday',
    ]
  });
}, RangeError);

assertThrowsInstanceOf(() => {
  gDN('foo-X', {
    keys: [
      'dates/gregorian/weekdays/thursday',
    ]
  });
}, RangeError);

const typeErrorKeys = [
  null,
  'string',
  Symbol.iterator,
  15,
  1,
  3.7,
  NaN,
  Infinity
];

for (let keys of typeErrorKeys) {
  assertThrowsInstanceOf(() => {
    gDN('en-US', {
      keys
    });
  }, TypeError);
}

const rangeErrorKeys = [
  [''],
  ['foo'],
  ['dates/foo'],
  ['/dates/foo'],
  ['dates/foo/foo'],
  ['dates/fields'],
  ['dates/fields/'],
  ['dates/fields/foo'],
  ['dates/fields/foo/month'],
  ['/dates/foo/faa/bar/baz'],
  ['dates///bar/baz'],
  ['dates/gregorian'],
  ['dates/gregorian/'],
  ['dates/gregorian/foo'],
  ['dates/gregorian/months'],
  ['dates/gregorian/months/foo'],
  ['dates/gregorian/weekdays'],
  ['dates/gregorian/weekdays/foo'],
  ['dates/gregorian/dayperiods'],
  ['dates/gregorian/dayperiods/foo'],
  ['dates/gregorian/months/الشهر'],
  [3],
  [null],
  ['d', 'a', 't', 'e', 's'],
  ['datesEXTRA'],
  ['dates/fieldsEXTRA'],
  ['dates/gregorianEXTRA'],
  ['dates/gregorian/monthsEXTRA'],
  ['dates/gregorian/weekdaysEXTRA'],
  ['dates/fields/dayperiods/amEXTRA'],
  ['dates/gregori\u1161n/months/january'],
  ["dates/fields/year/"],
  ["dates/fields/year\0"],
  ["dates/fields/month/"],
  ["dates/fields/month\0"],
  ["dates/fields/week/"],
  ["dates/fields/week\0"],
  ["dates/fields/day/"],
  ["dates/fields/day\0"],
  ["dates/gregorian/months/january/"],
  ["dates/gregorian/months/january\0"],
  ["dates/gregorian/weekdays/saturday/"],
  ["dates/gregorian/weekdays/saturday\0"],
  ["dates/gregorian/dayperiods/am/"],
  ["dates/gregorian/dayperiods/am\0"],
  ["dates/fields/months/january/"],
  ["dates/fields/months/january\0"],
];

for (let keys of rangeErrorKeys) {
  assertThrowsInstanceOf(() => {
    gDN('en-US', {
      keys
    });
  }, RangeError);
}

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
