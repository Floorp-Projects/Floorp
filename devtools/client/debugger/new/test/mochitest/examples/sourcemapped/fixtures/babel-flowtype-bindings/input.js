import type { One, Two, Three } from "./src/mod";

type Other = {};

const aConst = "a-const";

export default function root() {
  console.log("pause here", aConst, root);
}
