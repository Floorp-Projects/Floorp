export default function root() {
  let val = 1;

  switch (true) {
    case true:
      let val = 2;
      console.log("pause here", root);
    default: {
      let val = 3;
      console.log("pause here", root);
    }
  }
}
