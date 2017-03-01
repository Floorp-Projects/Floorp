function recurseA(i) {
  if (i == 20) {
    debugger;
    return;
  }

  // down into the rabbit hole we go
  return (i % 2) ? recurseA(++i) : recurseB(++i);
}

function recurseB(i) {
  if (i == 20) {
    debugger;
    return;
  }

  // down into the rabbit hole we go
  return (i % 2) ? recurseA(++i) : recurseB(++i);
}


function startRecursion() {
  return recurseA(0);
}
