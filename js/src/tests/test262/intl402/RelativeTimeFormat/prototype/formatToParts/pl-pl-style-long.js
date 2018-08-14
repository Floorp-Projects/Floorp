// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.RelativeTimeFormat.prototype.format
description: Checks the behavior of Intl.RelativeTimeFormat.prototype.format() in Polish.
features: [Intl.RelativeTimeFormat]
locale: [pl-PL]
---*/

function verifyFormatParts(actual, expected, message) {
  assert.sameValue(actual.length, expected.length, `${message}: length`);

  for (let i = 0; i < actual.length; ++i) {
    assert.sameValue(actual[i].type, expected[i].type, `${message}: parts[${i}].type`);
    assert.sameValue(actual[i].value, expected[i].value, `${message}: parts[${i}].value`);
    assert.sameValue(actual[i].unit, expected[i].unit, `${message}: parts[${i}].unit`);
  }
}

function regular(s) {
  return {
    "many": s,
    "few": s + "y",
    "one": s + "ę",
  }
}

// https://www.unicode.org/cldr/charts/33/summary/pl.html#1419
const units = {
  "second": regular("sekund"),
  "minute": regular("minut"),
  "hour": regular("godzin"),
  "day": {
    "many": "dni",
    "few": "dni",
    "one": "dzień",
  },
  "week": {
    "many": "tygodni",
    "few": "tygodnie",
    "one": "tydzień",
  },
  "month": {
    1000: "miesięcy",
    "many": "miesięcy",
    "few": "miesiące",
    "one": "miesiąc",
  },
  "quarter": {
    "many": "kwartałów",
    "few": "kwartały",
    "one": "kwartał",
  },
  "year": {
    "many": "lat",
    "few": "lata",
    "one": "rok",
  },
};

const rtf = new Intl.RelativeTimeFormat("pl-PL", {
  "style": "long",
});

assert.sameValue(typeof rtf.formatToParts, "function", "formatToParts should be supported");

for (const [unitArgument, expected] of Object.entries(units)) {
  verifyFormatParts(rtf.formatToParts(1000, unitArgument), [
    { "type": "literal", "value": "za " },
    { "type": "integer", "value": "1\u00a0000", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.many}` },
  ], `formatToParts(1000, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(10, unitArgument), [
    { "type": "literal", "value": "za " },
    { "type": "integer", "value": "10", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.many}` },
  ], `formatToParts(10, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(2, unitArgument), [
    { "type": "literal", "value": "za " },
    { "type": "integer", "value": "2", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.few}` },
  ], `formatToParts(2, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(1, unitArgument), [
    { "type": "literal", "value": "za " },
    { "type": "integer", "value": "1", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.one}` },
  ], `formatToParts(1, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(0, unitArgument), [
    { "type": "literal", "value": "za " },
    { "type": "integer", "value": "0", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.many}` },
  ], `formatToParts(0, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(-0, unitArgument), [
    { "type": "integer", "value": "0", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.many} temu` },
  ], `formatToParts(-0, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(-1, unitArgument), [
    { "type": "integer", "value": "1", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.one} temu` },
  ], `formatToParts(-1, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(-2, unitArgument), [
    { "type": "integer", "value": "2", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.few} temu` },
  ], `formatToParts(-2, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(-10, unitArgument), [
    { "type": "integer", "value": "10", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.many} temu` },
  ], `formatToParts(-10, ${unitArgument})`);

  verifyFormatParts(rtf.formatToParts(-1000, unitArgument), [
    { "type": "integer", "value": "1\u00a0000", "unit": unitArgument },
    { "type": "literal", "value": ` ${expected.many} temu` },
  ], `formatToParts(-1000, ${unitArgument})`);
}

reportCompare(0, 0);
