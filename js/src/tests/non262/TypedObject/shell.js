// Checks that |a_orig| and |b_orig| are:
//   1. Both instances of |type|, and
//   2. Are structurally equivalent (as dictated by the structure of |type|).
function assertTypedEqual(type, a_orig, b_orig) {
  try {
    recur(type, a_orig, b_orig);
  } catch (e) {
    var msg = `failure during assertTypedEqual(${describeType(type)}, ${JSON.stringify(a_orig)}, ${JSON.stringify(b_orig)})`;
    print(msg);
    throw e;
  }

  function recur(type, a, b) {
    if (type instanceof ArrayType) {
      assertEq(a.length, type.length);
      assertEq(b.length, type.length);
      for (var i = 0; i < type.length; i++)
        recur(type.elementType, a[i], b[i]);
      } else if (type instanceof StructType) {
        var fieldNames = Object.getOwnPropertyNames(type.fieldTypes);
        for (var i = 0; i < fieldNames.length; i++) {
          var fieldName = fieldNames[i];
          recur(type.fieldTypes[fieldName], a[fieldName], b[fieldName]);
        }
      } else {
        assertEq(a, b);
      }
  }
}

function describeType(type) {
  if (type instanceof TypedObject.ArrayType) {
    return `new ArrayType(${describeType(type.elementType)}, ${type.length})`;
  } else if (type instanceof TypedObject.StructType) {
    var fields = [];
    for (var key of Object.getOwnPropertyNames(type.fieldTypes)) {
      fields.push(`${key}: ${describeType(type.fieldTypes[key])}`);
    }
    return `new StructType({${fields.join(", ")}})`;
  } else {
    for (var key of Object.getOwnPropertyNames(TypedObject)) {
      if (TypedObject[key] === type) {
        return key;
      }
    }
    return JSON.stringify(type);
  }
}
