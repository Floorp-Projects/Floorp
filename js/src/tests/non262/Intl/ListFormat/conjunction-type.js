// |reftest| skip-if(!this.hasOwnProperty('Intl'))

// Note: Use the same test locales as used in unit-type.js

const {Element, Literal} = ListFormatParts;
const styles = ["long", "short", "narrow"];

// Test with zero elements.
{
    const list = [];
    const expected = [];
    const locales = ["ar", "de", "en", "es", "ja", "nl", "th", "zh"];

    for (let locale of locales) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "conjunction", style});
            assertParts(lf, list, expected);
        }
    }
}

// Test with one element.
{
    const list = ["A"];
    const expected = [Element(list[0])];
    const locales = ["ar", "de", "en", "es", "ja", "nl", "th", "zh"];

    for (let locale of locales) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "conjunction", style});
            assertParts(lf, list, expected);
        }
    }
}

// Test with two elements to cover the [[Template2]] case.
{
    const list = ["A", "B"];

    const testData = {
        "ar": { long: [Element("A"), Literal(" و"), Element("B")] },
        "de": { long: [Element("A"), Literal(" und "), Element("B")] },
        "en": {
            long: [Element("A"), Literal(" and "), Element("B")],
            short: [Element("A"), Literal(" & "), Element("B")],
            narrow: [Element("A"), Literal(", "), Element("B")],
        },
        "es": { long: [Element("A"), Literal(" y "), Element("B")] },
        "ja": { long: [Element("A"), Literal("、"), Element("B")] },
        "nl": {
            long: [Element("A"), Literal(" en "), Element("B")],
            short: [Element("A"), Literal(" & "), Element("B")],
            narrow: [Element("A"), Literal(", "), Element("B")],
        },
        "th": { long: [Element("A"), Literal("และ"), Element("B")] },
        "zh": {
          long: [Element("A"), Literal("和"), Element("B")],
          narrow: [Element("A"), Literal("、"), Element("B")],
        },
    };

    for (let [locale, localeData] of Object.entries(testData)) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "conjunction", style});
            let {[style]: expected = localeData.long} = localeData;
            assertParts(lf, list, expected);
        }
    }
}

// Test with more than two elements.
//
// Use four elements to cover all template parts ([[TemplateStart]], [[TemplateMiddle]], and
// [[TemplateEnd]]).
{
    const list = ["A", "B", "C", "D"];

    const testData = {
        "ar": {
            long: [Element("A"), Literal(" و"), Element("B"), Literal(" و"), Element("C"), Literal(" و"), Element("D")],
            short: [Element("A"), Literal(" و"), Element("B"), Literal(" و"), Element("C"), Literal(" و"), Element("D")],
            narrow: [Element("A"), Literal(" و"), Element("B"), Literal(" و"), Element("C"), Literal(" و"), Element("D")],
        },
        "de": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" und "), Element("D")],
        },
        "en": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", and "), Element("D")],
            short: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", & "), Element("D")],
            narrow: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", "), Element("D")],
        },
        "es": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" y "), Element("D")],
        },
        "ja": {
            long: [Element("A"), Literal("、"), Element("B"), Literal("、"), Element("C"), Literal("、"), Element("D")],
        },
        "nl": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" en "), Element("D")],
            short: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" & "), Element("D")],
            narrow: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", "), Element("D")],
        },
        "th": {
            long: [Element("A"), Literal(" "), Element("B"), Literal(" "), Element("C"), Literal(" และ"), Element("D")],
        },
        "zh": {
            long: [Element("A"), Literal("、"), Element("B"), Literal("、"), Element("C"), Literal("和"), Element("D")],
            narrow: [Element("A"), Literal("、"), Element("B"), Literal("、"), Element("C"), Literal("、"), Element("D")],
        },
    };

    for (let [locale, localeData] of Object.entries(testData)) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "conjunction", style});
            let {[style]: expected = localeData.long} = localeData;
            assertParts(lf, list, expected);
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
