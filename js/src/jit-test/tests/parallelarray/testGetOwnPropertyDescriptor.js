function GetOwnProperyDescriptor() {
    var p = new ParallelArray([9]);
    assertEq(Object.getOwnPropertyDescriptor(p, "length").toSource(), {configurable:false, enumerable:false, value:1, writable:false}.toSource());
    assertEq(Object.getOwnPropertyDescriptor(p, "0").toSource(), {configurable:false, enumerable:true, value:9, writable:false}.toSource());
}

GetOwnProperyDescriptor();

