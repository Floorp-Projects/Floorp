export default function root() {
  let one = 1;

  class Thing {}

  class Another {
    method() {
      let two = 2;

      const three = 3;

      console.log("pause here", Another, one, Thing, root);
    }
  }

  new Another().method();
}
