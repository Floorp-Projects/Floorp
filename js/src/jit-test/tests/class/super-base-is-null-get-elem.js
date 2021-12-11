class NullProto {
  static m(key) {
    try {
      return super[key];
    } catch {}
  }
}
Object.setPrototypeOf(NullProto, null);

for (var i = 0; i < 100; ++i) {
  NullProto.m(i);
}
