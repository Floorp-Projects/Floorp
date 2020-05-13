for (var t = 0; t < 10000; t++) {
    (t + "\u1234").split(/(.)\1/i);
}
