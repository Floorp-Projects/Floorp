// Iterating over a property with an XML id.
function n() {}
function g() {}
eval("\
  function a() {}\
  function b() {\
    for (w in this) {}\
    Object.defineProperty(\
      this, \
      new AttributeName, \
      ({enumerable: true})\
    )\
  }\
  for (z in [0, 0, 0]) b()\
")

// Test it doesn't assert.

