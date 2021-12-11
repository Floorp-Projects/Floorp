function test(str) {
  for (let i = 0; i < 100; ++i)
    Reflect.apply(String.prototype.substring, str, [])
}
enableGeckoProfilingWithSlowAssertions();
test("");
