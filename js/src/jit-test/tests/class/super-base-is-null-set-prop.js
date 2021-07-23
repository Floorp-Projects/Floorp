class NullProto {
  static m() {
    try {
      super.p = 0;
    } catch {}
  }
}
Object.setPrototypeOf(NullProto, null);

for (var i = 0; i < 100; ++i) {
  NullProto.m();
}
