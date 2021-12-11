// Set to true to emit ' +' instead of the unreadable '\s+'.
var SPACEDEBUG = false;

// Any hex string
var HEX = '[0-9a-fA-F]'
var HEXES = `${HEX}+`;

function wrap(options, funcs) {
    if ('memory' in options)
        return `(module (memory ${options.memory}) ${funcs})`;
    return `(module ${funcs})`;
}

function fixlines(s) {
    return s.split(/\n+/)
        .map(strip)
        .filter(x => x.length > 0)
        .map(x => '(?:0x)?' + HEXES + ' ' + x)
        .map(spaces)
        .join('\n');
}

function strip(s) {
    while (s.length > 0 && isspace(s.charAt(0)))
        s = s.substring(1);
    while (s.length > 0 && isspace(s.charAt(s.length-1)))
        s = s.substring(0, s.length-1);
    return s;
}

function spaces(s) {
    let t = '';
    let i = 0;
    while (i < s.length) {
        if (isspace(s.charAt(i))) {
            t += SPACEDEBUG ? ' +' : '\\s+';
            i++;
            while (i < s.length && isspace(s.charAt(i)))
                i++;
        } else {
            t += s.charAt(i++);
        }
    }
    return t;
}

function isspace(c) {
    return c == ' ' || c == '\t';
}
