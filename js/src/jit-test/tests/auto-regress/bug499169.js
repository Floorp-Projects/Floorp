// Binary: cache/js-dbg-32-30b481fd82f5-linux
// Flags: -j
//
var Native = function(k) {
    var f = k.initialize;
    var d = f;
    var j = function(n, l, o, m) {
        n.prototype[l] = o;
    };
    d.alias = function(n, l, o) {
        n = this.prototype[n]
        return j(this, l, n, o);
    };
    d.implement = function(m, l, o) {
        for (var n in m) {
            j(this, n, m[n], l);
        }
    }
    return d;
};
(function() {
    var a = {
        Array: Array,
        Function: Function,
    };
    for (var h in a) {
        new Native({
            initialize: a[h],
        });
    }
} ());
Array.alias("forEach", "each");
function $merge() {
    var a = Array.prototype.slice.call(arguments);
    a.unshift({});
    return $mixin.apply(null, a);
}
function $mixin(e) {
    for (var d = 1, a = arguments.length; d < a; d++) {
        var b = arguments[d];
        for (var c in b) {
            var g = b[c],
            f = e[c];
            e[c] = f && $type(g) == "object" && $type(f) == "object" ? $mixin(f, g) : $unlink(g);
        }
    }
}
function $type(a) {
    if (a == undefined) {
        return false;
    }
    if (a.$family) {
    }
    return typeof a;
}
function $unlink(c) {
    if ($type(c) == "object") {
        b = {};
    }
    return b;
}
var Window = new Native({
    initialize: function(a) {},
});
Array.implement({
    extend: function(c) {
        return this;
    }
});
Function.implement({
    extend: function(a) {
        for (var b in a) {
            this[b] = a[b];
        }
        return this;
    },
    run: function(a, b) {
        return this.apply(b, a);
    }
});
function Class(b) {
    var a = function() {
        Object.reset(this);
        var c = this.initialize ? this.initialize.apply(this, arguments) : this;
    }.extend(this);
    a.implement(b);
    return a;
}
Object.reset = function(a, c) {
    if (c == null) {
        for (var e in a) {
            Object.reset(a, e);
        }
    }
    switch ($type(a[c])) {
    case "object":
        var d = function() {};
        d.prototype = a[c];
        var b = new d;
        a[c] = Object.reset(b);
    }
    return a;
};
(new Native({initialize: Class})).extend({});
    function wrap(a, b, c) {
        return function() {
            var d = c.apply(this, arguments);
        }.extend({});
    }
Class.implement({
    implement: function(a, d) {
        if ($type(a) == "object") {
            for (var e in a) {
                this.implement(e, a[e]);
            }
        }
        var f = Class.Mutators[a];
        if (f) {
            d = f.call(this, d);
        }
        var c = this.prototype;
        switch ($type(d)) {
        case "function":
            c[a] = wrap(this, a, d);
            break;
        case "object":
            var b = c[a];
            if ($type(b) == "object") {
                $mixin(b, d);
            } else {
                c[a] = $unlink(d);
            }
        }
    }
});
Class.Mutators = {
    Extends: function(a) {
        this.prototype = new a;
    },
    Implements: function(a) {
        a.each(function(b) {
            b = new b;
            this.implement(b);
        },
        this);
    }
};
var Events = new Class({});
var Options = new Class({
    setOptions: function() {
        this.options = $merge.run([this.options].extend(arguments));
    }
});
var Autocompleter = {};
Autocompleter.Base = new Class({
    Implements: [Options, Events],
    options: {},
});
Autocompleter.Ajax = {};
Autocompleter.Ajax.Base = new Class({
    Extends: Autocompleter.Base,
    options: {
        postVar: "value",
        onRequest: function(){},
    }
});
Autocompleter.Ajax.Json = new Class({
    Extends: Autocompleter.Ajax.Base,
});
Autocompleter.JsonP = new Class({
    Extends: Autocompleter.Ajax.Json,
    options: {
        minLength: 1
    },
    initialize: function(el, url, options) {
        this.setOptions({});
    }
});
new Autocompleter.JsonP();
