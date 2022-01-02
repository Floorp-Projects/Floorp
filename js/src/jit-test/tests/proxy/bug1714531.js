var p = new Proxy({ get a() { } }, {
  defineProperty() {
    return true;
  }
});
Object.defineProperty(p, "a", { value: 1 });
