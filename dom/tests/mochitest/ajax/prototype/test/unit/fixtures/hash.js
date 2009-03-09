var Fixtures = {
  one: { a: 'A#' },
  
  many: {
    a: 'A',
    b: 'B',
    c: 'C',
    d: 'D#'
  },

  functions: {
    quad: function(n) { return n*n },
    plus: function(n) { return n+n }
  },
  
  multiple:         { color: $w('r g b') },
  multiple_nil:     { color: ['r', null, 'g', undefined, 0] },
  multiple_all_nil: { color: [null, undefined] },
  multiple_empty:   { color: [] },
  multiple_special: { 'stuff[]': $w('$ a ;') },

  value_undefined:  { a:"b", c:undefined },
  value_null:       { a:"b", c:null },
  value_zero:       { a:"b", c:0 }
};
