/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["UnitConverterSimple"];

// NOTE: This units table need to be localized upon supporting multi locales
//       since it supports en-US only.
//       e.g. Should support plugada or funty as well for pound.
const UNITS_GROUPS = [
  {
    // Angle
    degree: 1,
    deg: "degree",
    d: "degree",
    radian: Math.PI / 180.0,
    rad: "radian",
    r: "radian",
    gradian: 1 / 0.9,
    grad: "gradian",
    g: "gradian",
    minute: 60,
    min: "minute",
    m: "minute",
    second: 3600,
    sec: "second",
    s: "second",
    sign: 1 / 30.0,
    mil: 1 / 0.05625,
    revolution: 1 / 360.0,
    circle: 1 / 360.0,
    turn: 1 / 360.0,
    quadrant: 1 / 90.0,
    rightangle: 1 / 90.0,
    sextant: 1 / 60.0,
  },
  {
    // Force
    newton: 1,
    n: "newton",
    kilonewton: 0.001,
    kn: "kilonewton",
    "gram-force": 101.9716213,
    gf: "gram-force",
    "kilogram-force": 0.1019716213,
    kgf: "kilogram-force",
    "ton-force": 0.0001019716213,
    tf: "ton-force",
    exanewton: 1.0e-18,
    en: "exanewton",
    petanewton: 1.0e-15,
    PN: "petanewton",
    Pn: "petanewton",
    teranewton: 1.0e-12,
    tn: "teranewton",
    giganewton: 1.0e-9,
    gn: "giganewton",
    meganewton: 0.000001,
    MN: "meganewton",
    Mn: "meganewton",
    hectonewton: 0.01,
    hn: "hectonewton",
    dekanewton: 0.1,
    dan: "dekanewton",
    decinewton: 10,
    dn: "decinewton",
    centinewton: 100,
    cn: "centinewton",
    millinewton: 1000,
    mn: "millinewton",
    micronewton: 1000000,
    µn: "micronewton",
    nanonewton: 1000000000,
    nn: "nanonewton",
    piconewton: 1000000000000,
    pn: "piconewton",
    femtonewton: 1000000000000000,
    fn: "femtonewton",
    attonewton: 1000000000000000000,
    an: "attonewton",
    dyne: 100000,
    dyn: "dyne",
    "joule/meter": 1,
    "j/m": "joule/meter",
    "joule/centimeter": 100,
    "j/cm": "joule/centimeter",
    "ton-force-short": 0.0001124045,
    short: "ton-force-short",
    "ton-force-long": 0.0001003611,
    tonf: "ton-force-long",
    "kip-force": 0.0002248089,
    kipf: "kip-force",
    "pound-force": 0.2248089431,
    lbf: "pound-force",
    "ounce-force": 3.5969430896,
    ozf: "ounce-force",
    poundal: 7.2330138512,
    pdl: "poundal",
    pond: 101.9716213,
    p: "pond",
    kilopond: 0.1019716213,
    kp: "kilopond",
  },
  {
    // Length
    meter: 1,
    m: "meter",
    nanometer: 1000000000,
    micrometer: 1000000,
    millimeter: 1000,
    mm: "millimeter",
    centimeter: 100,
    cm: "centimeter",
    kilometer: 0.001,
    km: "kilometer",
    mile: 0.0006213689,
    mi: "mile",
    yard: 1.0936132983,
    yd: "yard",
    foot: 3.280839895,
    feet: "foot",
    ft: "foot",
    inch: 39.37007874,
    inches: "inch",
    in: "inch",
  },
  {
    // Mass
    kilogram: 1,
    kg: "kilogram",
    gram: 1000,
    g: "gram",
    milligram: 1000000,
    mg: "milligram",
    ton: 0.001,
    t: "ton",
    longton: 0.0009842073,
    "l.t.": "longton",
    "l/t": "longton",
    shortton: 0.0011023122,
    "s.t.": "shortton",
    "s/t": "shortton",
    pound: 2.2046244202,
    lbs: "pound",
    lb: "pound",
    ounce: 35.273990723,
    oz: "ounce",
    carat: 5000,
    ffd: 5000,
  },
];

// There are some units that will be same in lower case in same unit group.
// e.g. Mn: meganewton and mn: millinewton on force group.
// Handle them as case-sensitive.
const CASE_SENSITIVE_UNITS = ["PN", "Pn", "MN", "Mn"];

const NUMBER_REGEX = "\\d+(?:\\.\\d+)?\\s*";
const UNIT_REGEX = "[A-Za-zµ0-9_./-]+";

// NOTE: This regex need to be localized upon supporting multi locales
//       since it supports en-US input format only.
const QUERY_REGEX = new RegExp(
  `^(${NUMBER_REGEX})(${UNIT_REGEX})(?:\\s+in\\s+|\\s+to\\s+|\\s*=\\s*)(${UNIT_REGEX})`,
  "i"
);

const DECIMAL_PRECISION = 10;

/**
 * This module converts simple unit such as angle and length.
 */
class UnitConverterSimple {
  /**
   * Convert the given search string.
   *
   * @param {string} searchString
   * @returns {string} conversion result.
   */
  convert(searchString) {
    const regexResult = QUERY_REGEX.exec(searchString);
    if (!regexResult) {
      return null;
    }

    const target = findUnitGroup(regexResult[2], regexResult[3]);

    if (!target) {
      return null;
    }

    const { group, inputUnit, outputUnit } = target;
    const inputNumber = Number(regexResult[1]);
    const outputNumber = parseFloat(
      ((inputNumber / group[inputUnit]) * group[outputUnit]).toPrecision(
        DECIMAL_PRECISION
      )
    );

    try {
      return new Intl.NumberFormat("en-US", {
        style: "unit",
        unit: outputUnit,
        maximumFractionDigits: DECIMAL_PRECISION,
      }).format(outputNumber);
    } catch (e) {}

    return `${outputNumber} ${outputUnit}`;
  }
}

/**
 * Returns the suitable unit group and unit for the given two values.
 * If could not found suitable unit group, returns null.
 *
 * @param {string} inputUnit
 * @param {string} outputUnit
 * @returns {object}
 *   {
 *     group: one in UNITS_GROUPS,
 *     inputUnit: input unit,
 *     outputUnit: output unit,
 *   }
 */
function findUnitGroup(inputUnit, outputUnit) {
  inputUnit = toSuitableUnit(inputUnit);
  outputUnit = toSuitableUnit(outputUnit);

  const group = UNITS_GROUPS.find(ug => ug[inputUnit] && ug[outputUnit]);

  if (!group) {
    return null;
  }

  const inputValue = group[inputUnit];
  const outputValue = group[outputUnit];

  return {
    group,
    inputUnit: typeof inputValue === "string" ? inputValue : inputUnit,
    outputUnit: typeof outputValue === "string" ? outputValue : outputUnit,
  };
}

function toSuitableUnit(unit) {
  return CASE_SENSITIVE_UNITS.includes(unit) ? unit : unit.toLowerCase();
}
