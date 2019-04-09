function funcb(msg) {
  console.log(msg)
}

function a() {return true}
function b() {return false}

function statements() {
  debugger;
  { funcb(); funcb(); }
  console.log("yo"); console.log("yo");
  debugger; debugger;
}

function flow() {
  debugger;
  if (true) console.log("hi")
  var i = 0;
  while(i++ < 2) console.log(i);
}

function sequences() {
  debugger;

  const a = {
    a: 1
  }

  const b = [
    1,
    funcb()
  ]

  funcb({
    a: 1,
  })
}

function expressions() {
  debugger
  a() ? b() : b()
  const y = ` ${funcb()} ${funcb()}`;
}
