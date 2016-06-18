if (!('oomTest' in this))
    quit();

oomTest(() => byteSize({}));
oomTest(() => byteSize({ w: 1, x: 2, y: 3 }));
oomTest(() => byteSize({ w:1, x:2, y:3, z:4, a:6, 0:0, 1:1, 2:2 }));
oomTest(() => byteSize([1, 2, 3]));
oomTest(() => byteSize(function () {}));

function f1() {
  return 42;
}
oomTest(() => byteSizeOfScript(f1));

oomTest(() => byteSize("1234567"));
oomTest(() => byteSize("千早ぶる神代"));

let s = Symbol();
oomTest(() => byteSize(s));
