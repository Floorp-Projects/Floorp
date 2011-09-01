for (let z = 0; z < 2; z++) {
    with ({x: function () {}})
        f = x;
}
