import aDefault from "./src/mod1";

export default function root() {
  let one = 1;

  {
    const two = 2;

    var fn = function someName(arg) {
      console.log(this, arguments);
      console.log("pause here", aDefault, one, two, fn, arg);

      var inner = (arg) => { var body = "42"; console.log("pause here", body); };
      inner();
    };
    fn.call("this-value", "arg-value");
  }
}

// The build harness sets the wrong global, so just override it.
Promise.resolve().then(() => {
  window.webpackStandalone = root;
});
