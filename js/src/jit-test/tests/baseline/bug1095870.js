// |jit-test| --ion-eager;
for (var j = 0; j < 2; j++) {
    (false).__proto__ = 0
}
