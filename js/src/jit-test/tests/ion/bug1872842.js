class C {
  constructor() {
    this.c;
  }
  get c() {
    new this.constructor();
    for (let i = 0; i < 5; i++) {}
  }
}
try {
  new C();
} catch {}
