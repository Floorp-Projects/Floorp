// Self-hosted JS code shouldn't be used for error message.

try {
  new Set(...[1]);
} catch (e) {
  assertEq(e.message, "1 is not iterable");
}
