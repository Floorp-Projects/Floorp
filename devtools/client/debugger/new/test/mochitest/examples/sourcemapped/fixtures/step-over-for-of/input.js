const vals = [1, 2];

export default function root() {
  console.log("pause here");

  for (const val of vals) {
    console.log("pause again", val);
  }

  console.log("done");
}
