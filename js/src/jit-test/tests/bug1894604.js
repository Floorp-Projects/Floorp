class C {
  set f(val) {
    this.f = val;
    super.g++;
  }
}

let c = new C();
gczeal(14,50);
try {
  c.f = 1;
} catch {}
