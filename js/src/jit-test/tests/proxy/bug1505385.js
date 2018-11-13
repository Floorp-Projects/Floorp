// |jit-test| exitstatus: 3

function createSimpleMembrane(target) {
  function wrap(obj) {
    if (obj !== Object(obj)) return obj;
    let handler = new Proxy({}, {get: function(_, key) {
        return (_, ...args) => {
          try {
            return wrap(Reflect[key](obj, ...(args.map(wrap))));
          } catch(e) {
            throw wrap(e);
        }
      }
    }});
    return new Proxy(obj, handler);
  }
  return { wrapper: wrap(target) };
}
var o = {
  f: function(x) { return x },
};
var m = createSimpleMembrane(o);
var x = m.wrapper.f({a: 1});
var Object = TypedObject.Object;
1.1!=(x||p)
