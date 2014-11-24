if (typeof TypedObject === "undefined")
  quit();

A = Array.bind()
var {
    StructType
} = TypedObject
var A = new StructType({});
(function() {
    new A
    for (var i = 0; i < 9; i++) {}
})()
