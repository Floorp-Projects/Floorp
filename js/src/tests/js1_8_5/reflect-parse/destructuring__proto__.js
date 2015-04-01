// |reftest| skip-if(!xulRuntime.shell)
// |reftest| skip-if(!xulRuntime.shell)
// bug 963641

Reflect.parse("({ __proto__: null });");
Reflect.parse("var { __proto__: x } = obj;");
Reflect.parse("var [{ __proto__: y }] = obj;");
Reflect.parse("[{ __proto__: y }] = arr;");
Reflect.parse("({ __proto__: y } = obj);");

if (typeof reportCompare === "function")
  reportCompare(true, true);



print("Tests complete");
