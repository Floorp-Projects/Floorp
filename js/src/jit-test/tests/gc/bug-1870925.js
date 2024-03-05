// |jit-test| --no-ggc

let x = [[]];
for (let i = 0; i < 25; i++) {
  for (let j = 0; j < 25; j++) {
    (function () {
      x[i] | 0;
    })();
  }
}

verifyprebarriers();
bailAfter(1);
verifyprebarriers();
