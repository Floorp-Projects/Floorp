let g = newGlobal({ newCompartment: true });
g.parent = this;
g.eval(
  "(" +
    function () {
      Debugger(parent).onExceptionUnwind = function (frame) {
        frame.older;
      };
    } +
    ")()"
);
function f(x, y) {
  try {
    Object.setPrototypeOf(
      y,
      new Proxy(Object.getPrototypeOf(y), {
        get(a, b, c) {
          return undefined;
        },
      })
    );
  } catch (e) {}
}
function h(x, y) {
  f(x, y);
}
oomTest(function () {
  h("", undefined);
  h("", "");
  "".replaceAll();
});
