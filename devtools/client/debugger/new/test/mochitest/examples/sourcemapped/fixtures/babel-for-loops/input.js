export default function root() {
  let i = 0;

  for (let i = 1; i < 2; i++) {
    console.log("pause here", root);
  }

  for (let i in { 2: "" }) {
    console.log("pause here", root);
  }

  for (let i of [3]) {
    console.log("pause here", root);
  }
}
