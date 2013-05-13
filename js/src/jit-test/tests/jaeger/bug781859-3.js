function e() {
    try {
        var t = undefined;
    } catch (e) { }
    while (t)
        continue;
}
for (var i = 0; i < 20; i++)
  e();
