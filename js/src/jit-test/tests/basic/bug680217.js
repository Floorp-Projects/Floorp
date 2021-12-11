try {
    for (var BUGNUMBER = 0, sz = Math.pow(2, 12); i < sz; i++)
        str += '0,';
} catch (exc1) {}
var str = '[';
for (var i = 0, BUGNUMBER; i < sz; i++)
    str += '0,';
var obj = {
    p: { __proto__: null },
};
for (var i = 0; i < sz; i++)
    str += '0,';
gc();
