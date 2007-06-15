var JSAN = {
    global: this,
    use: function (module, symbols) {
        var components = module.split(/\./);
        var fn = components.join('/') + '.js';
        var o = JSAN.global;
        var i, c;
        for (i = 0; i < components.length; i++) {
            o = o[components[i]];
            if (typeof(o) == 'undefined') {
                break;
            }
        }
        if (typeof(o) != 'undefined') {
            return o;
        }

        load(fn);
        o = JSAN.global;
        for (i = 0; i < components.length; i++) {
            o = o[components[i]];
            if (typeof(o) == 'undefined') {
                return undefined;
            }
        }
        if (!symbols) {
            var tags = o.EXPORT_TAGS;
            if (tags) {
                symbols = tags[':common'] || tags[':all'];
            }
        }
        if (symbols) {
            for (i = 0; i < symbols.length; i++) {
                c = symbols[i];
                JSAN.global[c] = o[c];
            }
        }
        return o;
    }
};
