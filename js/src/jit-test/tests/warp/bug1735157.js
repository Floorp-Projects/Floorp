const options = ["", {}];
let value = "";
for (let i = 0; i < 10000; i++) {
    value += options[(Math.random() < 0.01) | 0];
}
