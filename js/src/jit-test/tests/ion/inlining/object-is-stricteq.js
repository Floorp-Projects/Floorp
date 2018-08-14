// Test when Object.is() is inlined as JSOP_STRICTEQ

function SameValue(x, y) {
    if (x === y) {
        return (x !== 0) || (1 / x === 1 / y);
    }
    return (x !== x && y !== y);
}

var compareTemplate = function compare(name, xs, ys) {
    // Compare each entry in xs with each entry in ys and ensure Object.is
    // computes the same result as SameValue.
    for (var i = 0; i < 1000; ++i) {
        var xi = (i % xs.length) | 0;
        var yi = ((i + ((i / ys.length) | 0)) % ys.length) | 0;

        assertEq(Object.is(xs[xi], ys[yi]), SameValue(xs[xi], ys[yi]), name);
    }
}

const objects = {
    plain: {},
    function: function(){},
    proxy: new Proxy({}, {}),
};

const sym = Symbol();

const testCases = [
    {
        name: "Homogenous-Int32",
        xs: [-1, 0, 1, 2, 0x7fffffff],
        ys: [2, 1, 0, -5, -0x80000000],
    },
    {
        name: "Homogenous-Boolean",
        xs: [true, false],
        ys: [true, false],
    },
    {
        name: "Homogenous-Object",
        xs: [{}, [], objects.plain, objects.proxy],
        ys: [{}, objects.function, objects.plain, objects.proxy],
    },
    {
        name: "Homogenous-String",
        xs: ["", "abc", "αβγαβγ", "𝐀𝐁𝐂𝐀𝐁𝐂", "ABCabc"],
        ys: ["abc", "ABC", "ABCABC", "αβγαβγ", "𝐀𝐁𝐂𝐀𝐁𝐂"],
    },
    {
        name: "Homogenous-Symbol",
        xs: [Symbol.iterator, Symbol(), Symbol.for("object-is"), sym],
        ys: [sym, Symbol.match, Symbol(), Symbol.for("object-is-two")],
    },
    {
        name: "Homogenous-Null",
        xs: [null, null],
        ys: [null, null],
    },
    {
        name: "Homogenous-Undefined",
        xs: [undefined, undefined],
        ys: [undefined, undefined],
    },

    // Note: Values must not include floating-point types!
    {
        name: "String-Value",
        xs: ["", "abc", "αβγαβγ", "𝐀𝐁𝐂𝐀𝐁𝐂"],
        ys: [null, undefined, sym, true, 0, "", {}],
    },
    {
        name: "Null-Value",
        xs: [null, null],
        ys: [null, undefined, sym, true, 0, "", {}],
    },
    {
        name: "Undefined-Value",
        xs: [undefined, undefined],
        ys: [null, undefined, sym, true, 0, "", {}],
    },
    {
        name: "Boolean-Value",
        xs: [true, false],
        ys: [null, undefined, sym, true, 0, "", {}],
    },

    // Note: Values must not include floating-point types!
    {
        name: "Value-String",
        xs: [null, undefined, sym, true, 0, "", {}],
        ys: ["", "abc", "αβγαβγ", "𝐀𝐁𝐂𝐀𝐁𝐂"],
    },
    {
        name: "Value-Null",
        xs: [null, undefined, sym, true, 0, "", {}],
        ys: [null, null],
    },
    {
        name: "Value-Undefined",
        xs: [null, undefined, sym, true, 0, "", {}],
        ys: [undefined, undefined],
    },
    {
        name: "Value-Boolean",
        xs: [null, undefined, sym, true, 0, "", {}],
        ys: [undefined, undefined],
    },

    // Strict-equal comparison can be optimized to bitwise comparison when
    // string types are not possible.
    // Note: Values must not include floating-point types!
    {
        name: "Value-Value",
        xs: [null, undefined, sym, true, 0, {}],
        ys: [null, undefined, sym, true, 0, {}],
    },
    {
        name: "ValueMaybeString-ValueMaybeString",
        xs: [null, undefined, sym, true, 0, "", {}],
        ys: [null, undefined, sym, true, 0, "", {}],
    },
    {
        name: "Value-ValueMaybeString",
        xs: [null, undefined, sym, true, 0, {}],
        ys: [null, undefined, sym, true, 0, "", {}],
    },
    {
        name: "ValueMaybeString-Value",
        xs: [null, undefined, sym, true, 0, "", {}],
        ys: [null, undefined, sym, true, 0, {}],
    },
];

for (let {name, xs, ys} of testCases) {
    // Create a separate function for each test case.
    // Use indirect eval to avoid possible direct eval deopts.
    const compare = (0, eval)(`(${compareTemplate})`);

    for (let i = 0; i < 5; ++i) {
        compare(name, xs, ys);
    }
}
