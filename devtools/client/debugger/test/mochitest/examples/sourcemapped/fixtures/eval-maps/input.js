
export default function root() {
  var one = 1;
  let two = 2;
  const three = 3;

  one;
  two;

  {
    let two = 4;
    const three = 5;

    console.log("pause here", one, two, three);
  }
}
