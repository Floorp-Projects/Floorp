// Bug 1702423

for (let a of []);
b = class {
    static [1] // Computed field name 1, no initializer.
}
