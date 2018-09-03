// |jit-test| --ion-limit-script-size=off; --ion-gvn=off
for (var i = 0; i < 1; ++i) {
    "".replace(/x/, "").replace(/y/, "12");
}
