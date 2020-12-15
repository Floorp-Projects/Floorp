// |reftest| skip-if(!this.hasOwnProperty("Intl"))

function IsIntlService(c) {
    return typeof c === "function" &&
           c.hasOwnProperty("prototype") &&
           c.prototype.hasOwnProperty("resolvedOptions");
}

function thisValues() {
    const intlConstructors = Object.getOwnPropertyNames(Intl).map(name => Intl[name]).filter(IsIntlService);

    return [
        // Primitive values.
        ...[undefined, null, true, "abc", Symbol(), 123],

        // Object values.
        ...[{}, [], /(?:)/, function(){}, new Proxy({}, {})],

        // Intl objects.
        ...[].concat(...intlConstructors.map(ctor => {
            let args = [];
            if (ctor === Intl.DisplayNames) {
                // Intl.DisplayNames can't be constructed without any arguments.
                args = [undefined, {type: "language"}];
            }

            return [
                // Instance of an Intl constructor.
                new ctor(...args),

                // Instance of a subclassed Intl constructor.
                new class extends ctor {}(...args),

                // Object inheriting from an Intl constructor prototype.
                Object.create(ctor.prototype),

                // Intl object not inheriting from its default prototype.
                Object.setPrototypeOf(new ctor(...args), Object.prototype),
            ];
        })),
    ];
}

// Intl.PluralRules cannot be invoked as a function.
assertThrowsInstanceOf(() => Intl.PluralRules(), TypeError);

// Also test with explicit this-value.
for (let thisValue of thisValues()) {
    assertThrowsInstanceOf(() => Intl.PluralRules.call(thisValue), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
