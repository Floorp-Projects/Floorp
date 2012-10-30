this.EXPORTED_SYMBOLS = ['checkFromJSM'];

this.checkFromJSM = function checkFromJSM(is_op) {
  is_op(new TextDecoder().encoding, "utf-8", "JSM should have TextDecoder");
  is_op(new TextEncoder().encoding, "utf-8", "JSM should have TextEncoder");
}
