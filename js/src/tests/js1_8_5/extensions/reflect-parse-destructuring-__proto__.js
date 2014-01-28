// |reftest| skip-if(!xulRuntime.shell)
// bug 963641

Reflect.parse("({ __proto__: null });");

if (typeof reportCompare === "function")
  reportCompare(true, true);



print("Tests complete");
