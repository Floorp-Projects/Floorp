function reportCompare (expected, actual, description) {}
function f()
{
  f(f, 0x09AA, 0x09B0, f);
}
try {
  reportCompare ("outer", f(),
                 "Inner function statement should not have been called.");
} catch (e) {}
