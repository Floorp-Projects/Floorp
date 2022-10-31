// |reftest| shell-option(--enable-shadow-realms) skip-if(!xulRuntime.shell)

// The output of Function.prototype.toString must match the |NativeFunction| production.
// https://tc39.es/ecma262/#sec-function.prototype.tostring
//
// NativeFunction :
//   function NativeFunctionAccessor? PropertyName[~Yield, ~Await]? ( FormalParameters[~Yield, ~Await] ) { [ native code ] }
//
// NativeFunctionAccessor :
//   get
//   set

function assertMatchesNativeFunction(f) {
  var source = f.toString();

  // Starts with "function".
  assertEq(/^function\b/.test(source), true);

  // Remove the optional |NativeFunctionAccessor| part.
  var nativeAccesorRe = /^(?<start>\s*function)(?<accessor>\s+[gs]et)(?<end>\s+[^(].*)$/;
  var match = nativeAccesorRe.exec(source);
  if (match) {
    source = match.groups.start + match.groups.end;
  }

  // The body must include the string "[native code".
  var closeCurly = source.lastIndexOf("}");
  var openCurly = source.lastIndexOf("{");
  assertEq(openCurly < closeCurly, true);

  var body = source.slice(openCurly + 1, closeCurly);
  assertEq(/^\s*\[native code\]\s*$/.test(body), true);

  // Verify |PropertyName| and |FormalParameters| are syntactically correct by parsing the source
  // code. But we first need to replace the "[native code]" substring.
  source = source.slice(0, openCurly) + "{}";

  // Also prepend "void" to parse the function source code as a FunctionExpression, because we
  // don't necessarily have a name part.
  source = "void " + source;

  try {
    Function(source);
  } catch {
    assertEq(true, false, `${source} doesn't match NativeFunction`);
  }
}

let sr = new ShadowRealm();
var f = sr.evaluate("function f() { }; f");

assertMatchesNativeFunction(f);

f.name = "koala"
assertMatchesNativeFunction(f);

Object.defineProperty(f, "name", { writable: true, value: "koala" });
assertMatchesNativeFunction(f);

f.name = "panda"
assertMatchesNativeFunction(f);

f.name = "has whitespace, therefore shouldn't match the PropertyName production";
assertMatchesNativeFunction(f);

f.name = 123;
assertMatchesNativeFunction(f);

Object.defineProperty(f, "name", { get() { throw new Error("unexpected side-effect"); } });
assertMatchesNativeFunction(f);

if (typeof reportCompare === 'function')
    reportCompare(true, true);
