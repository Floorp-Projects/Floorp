// |reftest| skip-if(!xulRuntime.shell)

// Retrieve the script file name through various functions and ensure it's
// always correctly decoded from UTF-8.

// Special value when filename cannot be retrieved.
const NOT_SUPPORTED = "*not supported*";

// Return the file name from the Error#fileName property.
function errorFileName(fileName) {
  return evaluate("new Error().fileName", {fileName});
}

// Return the file name from the Parser by a SyntaxError.
function errorFileNameParser(fileName) {
  try {
    evaluate("###", {fileName});
  } catch (e) {
    return e.fileName;
  }
}

// Retrieve the file name through DescriptedCaller (1).
function descriptedCallerViaThisFileName(fileName) {
  return evaluate("thisFilename()", {fileName});
}

// Retrieve the file name through DescriptedCaller (2).
function descriptedCallerViaEvalInContext(fileName) {
  return evaluate("evalcx('new Error().fileName')", {fileName});
}

// Retrieve the file name through DescriptedCaller (3).
function descriptedCallerViaEval(fileName) {
  var pattern = / line 1 > eval$/;
  return evaluate("eval('new Error().fileName')", {fileName}).replace(pattern, "");
}

// Retrieve the file name through DescriptedCaller (4).
function descriptedCallerViaFunction(fileName) {
  var pattern = / line 1 > Function$/;
  return evaluate("Function('return new Error().fileName')()", {fileName}).replace(pattern, "");
}

// Retrieve the file name through DescriptedCaller (5).
function descriptedCallerViaEvalReturningScope(fileName) {
  return evaluate("evalReturningScope('var a = new Error().fileName')", {fileName}).a;
}

// Retrieve the file name through DescriptedCaller (7).
var wasmModuleConstructorTemp;
function descriptedCallerViaWasm(fileName) {
  if (!wasmIsSupported()) {
    return fileName;
  }

  wasmModuleConstructorTemp = null;
  evaluate(`
    function wasmEvalText(str, imports) {
      let binary = wasmTextToBinary(str);
      assertEq(WebAssembly.validate(binary), true);
      let m = new WebAssembly.Module(binary);
      return new WebAssembly.Instance(m, imports);
    }
    wasmEvalText('(module (import "" "a" (func)) (func (call 0)) (export "bar" (func 1)))',
                  {
                    "": {
                      a() {
                        wasmModuleConstructorTemp = new Error().stack;
                        return 0;
                      }
                    }
                  }).exports.bar();
  `, {fileName});
  var pattern = /^@(.*) line \d+ >.*$/;
  var index = 1; // Direct caller is the wasm function.
  return wasmModuleConstructorTemp.split("\n")[index].replace(pattern, "$1");
}

// Return the file name from Reflect.parse().
function reflectParseSource(fileName) {
  return Reflect.parse("", {source: fileName}).loc.source;
}

// Return the file name using the Error#stack property.
function fromErrorStack(fileName) {
  var pattern = /^@(.*):\d+:\d+$/;
  return evaluate("new Error().stack", {fileName}).split("\n")[0].replace(pattern, "$1");
}

// Return the file name using the Error#stack property from an asm.js function.
function fromErrorStackAsmJS(fileName) {
  var asm = evaluate(`(function asm(stdlib, foreign) {
    "use asm";
    var f = foreign.f;
    function g() {
      return f() | 0;
    }
    return {g: g};
  })`, {fileName});

  var stackFileName;
  var foreign = {
    f() {
      var pattern = /^g@(.*):\d+:\d+$/;
      var index = 1; // Direct caller is the asm.js function.
      var stack = new Error().stack;
      stackFileName = stack.split("\n")[index].replace(pattern, "$1");
      return 0;
    }
  };

  asm(this, foreign).g();

  return stackFileName;
}

// Return the file name using the Error#stacl property when a streaming compiled WASM function.
function fromErrorStackStreamingWasm(fileName) {
  if (!wasmIsSupported() || helperThreadCount() == 0) {
    return fileName;
  }

  var source = new Uint8Array(wasmTextToBinary(`
    (module (import "" "a" (func)) (func (call 0)) (export "bar" (func 1)))
  `));
  source.url = fileName;

  var stackFileName;
  var imports = {
    "": {
      a() {
        var pattern = /^@(.*):wasm-function.*$/;
        var index = 1; // Direct caller is the asm.js function.
        var stack = new Error().stack;
        stackFileName = stack.split("\n")[index].replace(pattern, "$1");
        return 0;
      }
    }
  };

  var result;
  WebAssembly.instantiateStreaming(source, imports).then(r => result = r);

  drainJobQueue();

  result.instance.exports.bar();

  return stackFileName;
}

// Return the file name using the embedded info in getBacktrace().
function getBacktraceScriptName(fileName) {
  var pattern = /^\d+ <TOP LEVEL> \["(.*)":\d+:\d\]$/;
  return evaluate("getBacktrace()", {fileName}).split("\n")[0].replace(pattern, "$1");
}

// Return the file name from the coverage report.
function getLcovInfoScriptName(fileName) {
  var g = newGlobal();
  var scriptFiles =  g.evaluate("getLcovInfo()", {fileName})
                      .split("\n")
                      .filter(x => x.startsWith("SF:"));
  assertEq(scriptFiles.length, 1);
  return scriptFiles[0].substring(3);
}

// Return the file name from the error during module import.
function moduleResolutionError(fileName) {
  const a = parseModule(`import { x } from "b";`, fileName);
  const ma = registerModule("a", a);
  const b = parseModule(`export var y = 10;`);
  const mb = registerModule("b", b);

  try {
    moduleLink(ma);
  } catch (e) {
    return e.fileName;
  }
}

// Return the file name from the profiler stack.
function geckoInterpProfilingStack(fileName) {
  enableGeckoProfilingWithSlowAssertions();
  const stack = evaluate(`readGeckoInterpProfilingStack();`, { fileName });
  if (stack.length === 0) {
    return NOT_SUPPORTED;
  }
  const result = stack[0].dynamicString;
  disableGeckoProfiling();
  return result;
}

const testFunctions = [
  errorFileName,
  errorFileNameParser,
  descriptedCallerViaThisFileName,
  descriptedCallerViaEvalInContext,
  descriptedCallerViaEval,
  descriptedCallerViaFunction,
  descriptedCallerViaEvalReturningScope,
  descriptedCallerViaWasm,
  reflectParseSource,
  fromErrorStack,
  fromErrorStackAsmJS,
  fromErrorStackStreamingWasm,
  getBacktraceScriptName,
  moduleResolutionError,
];

if (isLcovEnabled()) {
  testFunctions.push(getLcovInfoScriptName);
}

const fileNames = [
  "",
  "test",
  "Ðëßþ",
  "тест",
  "テスト",
  "\u{1F9EA}",
];

for (const fn of testFunctions) {
  for (const fileName of fileNames) {
    const result = fn(fileName);
    if (result === NOT_SUPPORTED) {
      continue;
    }
    assertEq(result, fileName, `Caller '${fn.name}'`);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
