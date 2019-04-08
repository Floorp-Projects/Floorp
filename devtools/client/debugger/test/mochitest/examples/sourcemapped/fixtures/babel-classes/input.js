export default function root() {
  class Another {
    bound = () => {
      return this;
    }

    method() {
      let two = 2;

      console.log("pause here", Another, root);
    }
  }

  new Another().method();
}
