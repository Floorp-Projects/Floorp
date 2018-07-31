const mod = "module scoped";

export default function forOf() {
  for (const x of [1]) {
    doThing(x);
  }

  function doThing(arg) {
    // Avoid optimize out
    window.console;
  }
}
