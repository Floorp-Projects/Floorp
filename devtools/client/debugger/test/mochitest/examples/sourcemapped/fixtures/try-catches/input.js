export default function root() {
  const one = 1;

  try {
    throw "AnError";
  } catch (err) {
    let two = 2;
    console.log("pause here", root);
  }
}
