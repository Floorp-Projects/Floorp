export {};

switch (foo) {
  case "zero":
    var zero;
  case "one":
    let one;
  case "two":
    let two;
  case "three": {
    let three;
  }
}

switch (foo) {
  case "":
    function two(){}
}
switch (foo) {
  case "":
    class three {}
}
