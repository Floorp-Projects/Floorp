// Babel will optimize this for..of because it call tell the value is an array.
export default function root() {
  console.log("pause here");

  for (const val of [1, 2]) {
    console.log("pause again", val);
  }

  console.log("done");
}
