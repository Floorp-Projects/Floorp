// Single line comment <<=== this is line 1
console.log("body instruction");
/* Multi 
* line
* comment
*/
function bodyFunction() {
  // Inline comment
  /* Inline
   * multiline comment
   */
  function inlineFunction() {
    console.log("nested instruction");
  }
  const inlineCount = 1;
  let inlineLet = 2;
  var inlineVar = 3;
  console.log("inline instruction");
}
const bodyCount = 1;
let bodyLet = 2;
var bodyVar = 3;
