gczeal(9);
let o = {p1:0, p2:0, set p3({}) {}, p4:1, p5:1,
         p6:1, p7:1, p8:1, p9:1, p10:1, p11:1};
for (let p in o)
    x = o[p];
delete o.p3;
for (let i = 0; i < 100; i++)
    x = -o;
