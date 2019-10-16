// |reftest| skip-if(!this.hasOwnProperty('Intl')||(!this.Intl.ListFormat&&!this.hasOwnProperty('addIntlExtras')))

// Note: This is a stripped down version of conjunction-type.js. When we support
// "short" and "narrow" styles, we can remove this file.

const {Element, Literal} = ListFormatParts;

// Test with zero elements.
{
    const list = [];
    const expected = [];
    const locales = ["ar", "de", "en", "es", "ja", "nl", "th", "zh"];

    for (let locale of locales) {
        let lf = new Intl.ListFormat(locale, {type: "conjunction", style: "long"});
        assertParts(lf, list, expected);
    }
}

// Test with one element.
{
    const list = ["A"];
    const expected = [Element(list[0])];
    const locales = ["ar", "de", "en", "es", "ja", "nl", "th", "zh"];

    for (let locale of locales) {
        let lf = new Intl.ListFormat(locale, {type: "conjunction", style: "long"});
        assertParts(lf, list, expected);
    }
}

// Test with two elements to cover the [[Template2]] case.
{
    const list = ["A", "B"];

    const testData = {
        "ar": [Element("A"), Literal(" و"), Element("B")],
        "de": [Element("A"), Literal(" und "), Element("B")],
        "en": [Element("A"), Literal(" and "), Element("B")],
        "es": [Element("A"), Literal(" y "), Element("B")],
        "ja": [Element("A"), Literal("、"), Element("B")],
        "nl": [Element("A"), Literal(" en "), Element("B")],
        "th": [Element("A"), Literal("และ"), Element("B")],
        "zh": [Element("A"), Literal("和"), Element("B")],
    };

    for (let [locale, expected] of Object.entries(testData)) {
        let lf = new Intl.ListFormat(locale, {type: "conjunction", style: "long"});
        assertParts(lf, list, expected);
    }
}

// Test with more than two elements.
//
// Use four elements to cover all template parts ([[TemplateStart]], [[TemplateMiddle]], and
// [[TemplateEnd]]).
{
    const list = ["A", "B", "C", "D"];

    const testData = {
        "ar": [Element("A"), Literal("، "), Element("B"), Literal("، "), Element("C"), Literal("، و"), Element("D")],
        "de": [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" und "), Element("D")],
        "en": [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(", and "), Element("D")],
        "es": [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" y "), Element("D")],
        "ja": [Element("A"), Literal("、"), Element("B"), Literal("、"), Element("C"), Literal("、"), Element("D")],
        "nl": [Element("A"), Literal(", "), Element("B"), Literal(", "), Element("C"), Literal(" en "), Element("D")],
        "th": [Element("A"), Literal(" "), Element("B"), Literal(" "), Element("C"), Literal(" และ"), Element("D")],
        "zh": [Element("A"), Literal("、"), Element("B"), Literal("、"), Element("C"), Literal("和"), Element("D")],
    };

    for (let [locale, expected] of Object.entries(testData)) {
        let lf = new Intl.ListFormat(locale, {type: "conjunction", style: "long"});
        assertParts(lf, list, expected);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
