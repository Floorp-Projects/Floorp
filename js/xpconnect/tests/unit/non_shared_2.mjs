globalThis["loaded"].push(2);

export function setGlobal(name, value) {
  globalThis[name] = value;
}
