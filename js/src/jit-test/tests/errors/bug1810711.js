var g = newGlobal({newCompartment: true});

try {
  undef()
} catch (err) {
  const handler = { "getPrototypeOf": (x) => () => x };
  const proxy = new g.Proxy(err, handler);
  try {
    proxy.stack
  } catch {}
}
