function GenericPartCreator(type) {
    return str => ({ type, value: str });
}

const NumberFormatParts = {
    Nan: GenericPartCreator("nan"),
    Inf: GenericPartCreator("infinity"),
    Integer: GenericPartCreator("integer"),
    Group: GenericPartCreator("group"),
    Decimal: GenericPartCreator("decimal"),
    Fraction: GenericPartCreator("fraction"),
    MinusSign: GenericPartCreator("minusSign"),
    PlusSign: GenericPartCreator("plusSign"),
    PercentSign: GenericPartCreator("percentSign"),
    Currency: GenericPartCreator("currency"),
    Literal: GenericPartCreator("literal"),
    ExponentSeparator: GenericPartCreator("exponentSeparator"),
    ExponentMinusSign: GenericPartCreator("exponentMinusSign"),
    ExponentInteger: GenericPartCreator("exponentInteger"),
    Compact: GenericPartCreator("compact"),
    Unit: GenericPartCreator("unit"),
};

function NumberRangeFormatParts(source) {
  let entries = Object.entries(NumberFormatParts)

  entries.push(["Approx", GenericPartCreator("approximatelySign")]);

  return Object.fromEntries(entries.map(([key, part]) => {
    let partWithSource = str => {
      return Object.defineProperty(part(str), "source", {
        value: source, writable: true, enumerable: true, configurable: true
      });
    };
    return [key, partWithSource];
  }));
}

function assertParts(nf, x, expected) {
    var parts = nf.formatToParts(x);
    assertEq(parts.map(part => part.value).join(""), nf.format(x),
             "formatToParts and format must agree");

    var len = parts.length;
    assertEq(len, expected.length, "parts count mismatch");
    for (var i = 0; i < len; i++) {
        assertEq(parts[i].type, expected[i].type, "type mismatch at " + i);
        assertEq(parts[i].value, expected[i].value, "value mismatch at " + i);
    }
}

function assertRangeParts(nf, start, end, expected) {
    var parts = nf.formatRangeToParts(start, end);
    assertEq(parts.map(part => part.value).join(""), nf.formatRange(start, end),
             "formatRangeToParts and formatRange must agree");

    var len = parts.length;
    assertEq(len, expected.length, "parts count mismatch");
    for (var i = 0; i < len; i++) {
        assertEq(parts[i].type, expected[i].type, "type mismatch at " + i);
        assertEq(parts[i].value, expected[i].value, "value mismatch at " + i);
        assertEq(parts[i].source, expected[i].source, "source mismatch at " + i);
    }
}

function runNumberFormattingTestcases(testcases) {
    for (let {locale, options, values} of testcases) {
        let nf = new Intl.NumberFormat(locale, options);

        for (let {value, string, parts} of values) {
            assertEq(nf.format(value), string,
                     `locale=${locale}, options=${JSON.stringify(options)}, value=${value}`);

            assertParts(nf, value, parts);
        }
    }
}
