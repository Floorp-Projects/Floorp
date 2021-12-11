class NullProto {
  static m() {
    try {
      return super.p;
    } catch {}
  }
}
Object.setPrototypeOf(NullProto, null);

for (var i = 0; i < 100; ++i) {
  NullProto.m();
}
