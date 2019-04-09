export default function root() {
  function fn(arg) {
    console.log(this, arguments);
    console.log("pause here", fn, root);

    const arrow = argArrow => {
      console.log(this, arguments);
      console.log("pause here", fn, root);
    };
    arrow("arrow-arg");
  }

  fn.call("this-value", "arg-value");
}
