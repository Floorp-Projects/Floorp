X = { value: "" }
z = 0;
for (var i = 0; i < 20; i++) {
    Object.defineProperty(this, "z", X);
    z = 1;
}
