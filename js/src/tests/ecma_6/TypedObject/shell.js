// Checks that |a_orig| and |b_orig| are:
//   1. Both instances of |type|, and
//   2. Are structurally equivalent (as dictated by the structure of |type|).
function assertTypedEqual(type, a_orig, b_orig) {
  try {
    recur(type, a_orig, b_orig);
  } catch (e) {
    print("failure during "+
          "assertTypedEqual("+type.toSource()+", "+a_orig.toSource()+", "+b_orig.toSource()+")");
    throw e;
  }

  function recur(type, a, b) {
    if (type instanceof ArrayType) {
      assertEq(a.length, type.length);
      assertEq(b.length, type.length);
      for (var i = 0; i < type.length; i++)
        recur(type.elementType, a[i], b[i]);
      } else if (type instanceof StructType) {
        for (var idx in type.fieldNames) {
          var fieldName = type.fieldNames[idx];
          if (type.fieldTypes[fieldName] !== undefined) {
            recur(type.fieldTypes[fieldName], a[fieldName], b[fieldName]);
          } else {
            throw new Error("assertTypedEqual no type for "+
                            "fieldName: "+fieldName+" in type: "+type.toSource());
          }
        }
      } else {
        assertEq(a, b);
      }
  }
}
