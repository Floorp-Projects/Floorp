// |jit-test| slow;

try {
    x = <x><y/></x>
    x += /x /
} catch (e) {}
for each(a in [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0]) {
    x += x;
}
default xml namespace = x;
<x></x>
