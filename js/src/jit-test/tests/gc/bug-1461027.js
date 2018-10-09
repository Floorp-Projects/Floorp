// |jit-test| skip-if: !this.hasOwnProperty("TypedObject")

for (var i = 0; i < 99; i++) {
	    w = new TypedObject.ArrayType(TypedObject.int32, 100).build(function() {});
}
relazifyFunctions();
