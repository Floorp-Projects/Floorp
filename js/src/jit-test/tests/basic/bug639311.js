/* Avoid use-after-free while sweeping type objects. */

try {
  Reflparse("")
} catch(e) {}
Reflect.parse("for(var a;a;j){if(a%2==0){c()}}")
try {
  (function() {
    for (a = 0;; j) {
      gc()
    }
  })()
} catch(e) {
  delete this.Math
}
gc()
Reflect.parse("{ let x; }")
gc()
