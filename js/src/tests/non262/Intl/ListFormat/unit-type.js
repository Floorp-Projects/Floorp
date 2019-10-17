// |reftest| skip -- "unit" type currently not supported

const {Element, Literal} = ListFormatParts;
const styles = ["long", "short", "narrow"];

// Test with zero elements.
{
    const list = [];
    const expected = [];
    const locales = ["ar", "de", "en", "es", "ja", "nl", "th", "zh"];

    for (let locale of locales) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "unit", style});
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
            let lf = new Intl.ListFormat(locale, {type: "unit", style});
            assertParts(lf, list, expected);
        }
    }
}

// Test with two elements to cover the [[Template2]] case.
{
    const list = ["A", "B"];

    const testData = {
        "ar": {
            long: [Element("A"), Literal(" و"), Element("B")],
            narrow: [Element("A"), Literal("، "), Element("B")],
        },
        "de": {
            long: [Element("A"), Literal(", "), Element("B")],
        },
        "en": {
            long: [Element("A"), Literal(", "), Element("B")],
            narrow: [Element("A"), Literal(" "), Element("B")],
        },
        "es": {
            long: [Element("A"), Literal(" y "), Element("B")],
            narrow: [Element("A"), Literal(" "), Element("B")],
        },
        "ja": {
            long: [Element("A"), Literal(" "), Element("B")],
            narrow: [Element("A"), Element("B")],
        },
        "nl": {
            long: [Element("A"), Literal(" en "), Element("B")],
            short: [Element("A"), Literal(", "), Element("B")],
            narrow: [Element("A"), Literal(", "), Element("B")],
        },
        "th": {
            long: [Element("A"), Literal(" และ "), Element("B")],
            short: [Element("A"), Literal(" "), Element("B")],
            narrow: [Element("A"), Literal(" "), Element("B")],
        },
        "zh": {
            long: [Element("A"), Element("B")],
        },
    };

    for (let [locale, localeData] of Object.entries(testData)) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "unit", style});
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
        // non-ASCII case
        "ar": {
            long: [Element("A"), Literal("، و"), Element("B"), Literal("، و"), Element("C"), Literal("، و"), Element("D")],
            narrow: [Element("A"), Literal("، "), Element("B"), Literal("، "), Element("C"), Literal("، "), Element("D")],
        },

        // all values are equal
        "de": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" und "), Element("D")],
        },

        // long and short values are equal
        "en": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", "), Element("D")],
            narrow: [Element("A"), Literal(" "), Element("B"), Literal(" "), Element("C"), Literal(" "), Element("D")],
        },

        // all values are different
        "es": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" y "), Element("D")],
            short: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", "), Element("D")],
            narrow: [Element("A"), Literal(" "), Element("B"), Literal(" "), Element("C"), Literal(" "), Element("D")],
        },

        // no spacing for narrow case
        "ja": {
            long: [Element("A"), Literal(" "), Element("B"), Literal(" "), Element("C"), Literal(" "), Element("D")],
            narrow: [Element("A"), Element("B"), Element("C"), Element("D")],
        },

        // short and narrow values are equal
        "nl": {
            long: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" en "), Element("D")],
            short: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", "), Element("D")],
            narrow: [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", "), Element("D")],
        },

        // another non-ASCII case
        "th": {
            long: [Element("A"), Literal(" "), Element("B"), Literal(" "), Element("C"), Literal(" และ "), Element("D")],
            narrow: [Element("A"), Literal(" "), Element("B"), Literal(" "), Element("C"), Literal(" "), Element("D")],
        },

        // no whitespace at all
        "zh": {
            long: [Element("A"), Element("B"), Element("C"), Element("D")],
        },
    };

    for (let [locale, localeData] of Object.entries(testData)) {
        for (let style of styles) {
            let lf = new Intl.ListFormat(locale, {type: "unit", style});
            let {[style]: expected = localeData.long} = localeData;
            assertParts(lf, list, expected);
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
