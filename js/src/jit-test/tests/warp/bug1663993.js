function r(relazify) {
    "foo".substr(0);
    if (relazify) relazifyFunctions();
}

for (var i = 0; i < 10; i++) {
    r(i == 9);
    r("");
}
