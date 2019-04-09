var foo = 1;
let bar = 2;
const baz = 3;
const a = 4,
  b = 5;
a = 5;

var { foo: { bar } } = {}
var {baz} = {}
var {ll = 3} = {}


var [first] = [1]

var { a: _a } = 3

var [oh, {my: god}] = [{},{}]

var [[oh], [{oy, vey: _vey, mitzvot: _mitz = 4}]] = [{},{}]

var [one, ...stuff] = []
