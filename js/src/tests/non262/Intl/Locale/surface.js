// |reftest| skip-if(!this.hasOwnProperty('Intl')||(!this.Intl.Locale&&!this.hasOwnProperty('addIntlExtras')))

function assertProperty(object, name, desc) {
    assertEq(desc === undefined || (typeof desc === "object" && desc !== null), true,
             "desc is a property descriptor");

    var actual = Object.getOwnPropertyDescriptor(object, name);
    if (desc === undefined) {
        assertEq(actual, desc, `property ${String(name)} is absent`);
        return;
    }
    assertEq(actual !== undefined, true, `property ${String(name)} is present`);

    var fields = ["value", "writable", "enumerable", "configurable", "get", "set"];
    for (var field of fields) {
        if (Object.prototype.hasOwnProperty.call(desc, field)) {
            assertEq(actual[field], desc[field], `unexpected value for ${field}`);
        }
    }
}

function assertBuiltinFunction(fn, length, name) {
    assertProperty(fn, "length", {
       value: length, writable: false, enumerable: false, configurable: true,
    });
}

function assertBuiltinMethod(object, propName, length, name) {
    var desc = Object.getOwnPropertyDescriptor(object, propName);
    assertProperty(object, propName, {
        value: desc.value, writable: true, enumerable: false, configurable: true
    });
    assertBuiltinFunction(desc.value, length, name);
}

function assertBuiltinGetter(object, propName, length, name) {
    var desc = Object.getOwnPropertyDescriptor(object, propName);

    assertBuiltinFunction(desc.get, length, name);
}

// Intl.Locale( tag[, options] )
assertBuiltinFunction(Intl.Locale, 1, "Locale");

// Properties of the Intl.Locale Constructor

// Intl.Locale.prototype
assertProperty(Intl.Locale, "prototype", {
    value: Intl.Locale.prototype, writable: false, enumerable: false, configurable: false,
});

// Properties of the Intl.Locale Prototype Object

// Intl.Locale.prototype.constructor
assertProperty(Intl.Locale.prototype, "constructor", {
    value: Intl.Locale, writable: true, enumerable: false, configurable: true,
});

// Intl.Locale.prototype[ @@toStringTag ]
assertProperty(Intl.Locale.prototype, Symbol.toStringTag, {
    value: "Intl.Locale", writable: false, enumerable: false, configurable: true,
});

// Intl.Locale.prototype.toString ()
assertBuiltinMethod(Intl.Locale.prototype, "toString", 0, "toString");

// get Intl.Locale.prototype.baseName
assertBuiltinGetter(Intl.Locale.prototype, "baseName", 0, "get baseName");

// get Intl.Locale.prototype.calendar
assertBuiltinGetter(Intl.Locale.prototype, "calendar", 0, "get calendar");

// get Intl.Locale.prototype.collation
assertBuiltinGetter(Intl.Locale.prototype, "collation", 0, "get collation");

// get Intl.Locale.prototype.hourCycle
assertBuiltinGetter(Intl.Locale.prototype, "hourCycle", 0, "get hourCycle");

// get Intl.Locale.prototype.caseFirst
assertBuiltinGetter(Intl.Locale.prototype, "caseFirst", 0, "get caseFirst");

// get Intl.Locale.prototype.numeric
assertBuiltinGetter(Intl.Locale.prototype, "numeric", 0, "get numeric");

// get Intl.Locale.prototype.numberingSystem
assertBuiltinGetter(Intl.Locale.prototype, "numberingSystem", 0, "get numberingSystem");

// get Intl.Locale.prototype.language
assertBuiltinGetter(Intl.Locale.prototype, "language", 0, "get language");

// get Intl.Locale.prototype.script
assertBuiltinGetter(Intl.Locale.prototype, "script", 0, "get script");

// get Intl.Locale.prototype.region
assertBuiltinGetter(Intl.Locale.prototype, "region", 0, "get region");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
