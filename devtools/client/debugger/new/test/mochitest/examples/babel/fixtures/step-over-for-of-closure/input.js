// Babel will convert the loop body to a function to handle the 'val' lexical
// enclosure behavior.
const vals = [1, 2];

export default function root() {
  console.log("pause here");

  for (const val of vals) {
    console.log("pause again", (() => val)());
  }

  console.log("done");
}
