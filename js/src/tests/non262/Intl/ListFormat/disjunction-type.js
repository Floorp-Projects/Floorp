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
            let lf = new Intl.ListFormat(locale, {type: "disjunction", style});
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
            let lf = new Intl.ListFormat(locale, {type: "disjunction", style});
            assertParts(lf, list, expected);
        }
    }
}

// Test with two elements to cover the [[Template2]] case.
{
    const list = ["A", "B"];

    const testData = {
        "ar": { long: [Element("A"), Literal(" أو "), Element("B")] },
        "de": { long: [Element("A"), Literal(" oder "), Element("B")] },
        "en": { long: [Element("A"), Literal(" or "), Element("B")] },
        "es": { long: [Element("A"), Literal(" o "), Element("B")] },
        "ja": { long: [Element("A"), Literal("または"), Element("B")] },
        "nl": { long: [Element("A"), Literal(" of "), Element("B")] },
        "th": {
          long: [Element("A"), Literal(" หรือ "), Element("B")],
          short: [Element("A"), Literal("หรือ"), Element("B")],
          narrow: [Element("A"), Literal("หรือ"), Element("B")],
        },
        "zh": { long: [Element("A"), Literal("或"), Element("B")] },
    };

    for (let [locale, localeData] of Object.entries(testData)) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "disjunction", style});
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
            long: [Element("A"), Literal(" أو "), Element("B"), Literal(" أو "), Element("C"), Literal(" أو "), Element("D")],
        },
        "de": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" oder "), Element("D")],
        },
        "en": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", or "), Element("D")],
        },
        "es": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" o "), Element("D")],
        },
        "ja": {
            long: [Element("A"), Literal("、"), Element("B"), Literal("、"), Element("C"), Literal("、または"), Element("D")],
        },
        "nl": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" of "), Element("D")],
        },
        "th": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" หรือ "), Element("D")],
        },
        "zh": {
            long: [Element("A"), Literal("、"), Element("B"), Literal("、"), Element("C"), Literal("或"), Element("D")],
        },
    };

    for (let [locale, localeData] of Object.entries(testData)) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "disjunction", style});
            let {[style]: expected = localeData.long} = localeData;
            assertParts(lf, list, expected);
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
