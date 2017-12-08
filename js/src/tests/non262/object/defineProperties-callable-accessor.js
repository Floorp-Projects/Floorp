// ObjectDefineProperties with non callable accessor throws.
const descriptors = [
    {get: 1}, {set: 1},
    {get: []}, {set: []},
    {get: {}}, {set: {}},
    {get: new Number}, {set: new Number},

    {get: 1, set: 1},
    {get: [], set: []},
    {get: {}, set: {}},
    {get: new Number, set: new Number},
];

for (const descriptor of descriptors) {
    assertThrowsInstanceOf(() => Object.create(null, {x: descriptor}), TypeError);
    assertThrowsInstanceOf(() => Object.defineProperties({}, {x: descriptor}), TypeError);
}

reportCompare(true, true);
