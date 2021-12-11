function toPrinted(value) {
  value = String(value);
}
String = Array;
toPrinted(123);
evaluate('toPrinted("foo");');
