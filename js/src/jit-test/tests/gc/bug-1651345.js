function main() {
  const v3 = {get:Object};
  const v5 = Object.defineProperty(Object,0,v3);
  addMarkObservers(Object)
  gc();
}
main();
