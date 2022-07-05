function assertArrayEq(value, expected)
{
    assertEq(value instanceof Array, true);
    assertEq(value.length, expected.length);
    for (let i = 0; i < value.length; i++)
        assertEq(value[i], expected[i]);
}

registerModule('a', parseModule(`
    export const [] = [];
    export const [a=0] = [];
    export const [b] = [1];
    export const [c, d, e] = [2, 3, 4];
    export const [, f, g] = [5, 6, 7];
    export const [h,, i] = [8, 9, 10];
    export const [,, j] = [11, 12, 13];
    export const [...k] = [14, 15, 16];
    export const [l, ...m] = [17, 18, 19];
    export const [,, ...n] = [20, 21, 22];
`));

m = parseModule(`
    import * as a from 'a';

    assertEq(a.a, 0);
    assertEq(a.b, 1);
    assertEq(a.c, 2);
    assertEq(a.d, 3);
    assertEq(a.e, 4);
    assertEq(a.f, 6);
    assertEq(a.g, 7);
    assertEq(a.h, 8);
    assertEq(a.i, 10);
    assertEq(a.j, 13);
    assertArrayEq(a.k, [14, 15, 16]);
    assertEq(a.l, 17);
    assertArrayEq(a.m, [18, 19]);
    assertArrayEq(a.n, [22]);
`);

moduleLink(m);
moduleEvaluate(m);

registerModule('o', parseModule(`
    export const {} = {};
    export const {x: a=0} = {};
    export const {x: b=0} = {x: 1};
    export const {c, d, e} = {c: 2, d: 3, e: 4};
    export const {x: f} = {x: 5};
    export const {g} = {};
    export const {h=6} = {};
`));

m = parseModule(`
    import * as o from 'o';

    assertEq(o.a, 0);
    assertEq(o.b, 1);
    assertEq(o.c, 2);
    assertEq(o.d, 3);
    assertEq(o.e, 4);
    assertEq(o.f, 5);
    assertEq(o.g, undefined);
    assertEq(o.h, 6);
`);

moduleLink(m);
moduleEvaluate(m);

registerModule('ao', parseModule(`
    export const [{x: a}, {x: b}] = [{x: 1}, {x: 2}];
    export const [{c}, {d}] = [{c: 3}, {d: 4}];
    export const [,, {e}, ...f] = [5, 6, {e: 7}, 8, 9];

    export const {x: [g, h, i]} = {x: [10, 11, 12]};

    export const [[], [...j], [, k, l]] = [[], [13, 14], [15, 16, 17]];

    export const {x: {m, n, o=20}, y: {z: p}} = {x: {m: 18, n: 19}, y: {z: 21}};
`));

m = parseModule(`
    import * as ao from 'ao';

    assertEq(ao.a, 1);
    assertEq(ao.b, 2);
    assertEq(ao.c, 3);
    assertEq(ao.d, 4);
    assertEq(ao.e, 7);
    assertArrayEq(ao.f, [8, 9]);
    assertEq(ao.g, 10);
    assertEq(ao.h, 11);
    assertEq(ao.i, 12);
    assertArrayEq(ao.j, [13, 14]);
    assertEq(ao.k, 16);
    assertEq(ao.l, 17);
    assertEq(ao.m, 18);
    assertEq(ao.n, 19);
    assertEq(ao.o, 20);
    assertEq(ao.p, 21);
`);

moduleLink(m);
moduleEvaluate(m);
