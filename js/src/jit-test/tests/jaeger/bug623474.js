for (var j=0;j<2;++j) { (function(o){o.length})(String.prototype); }

for each(let y in [Number, Number]) {
    try {
        "".length()
    } catch(e) {}
}

/* Don't crash. */

