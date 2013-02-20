function concat(v, index, array) {}
var strings = ['hello', 'Array', 'WORLD'];
try {
    strings.forEach();
} catch(e) {
    strings.forEach(concat);
}
