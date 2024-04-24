// |jit-test| skip-if: !wasmJSStringBuiltinsEnabled();

let testModule = `(module
  (type $arrayMutI16 (array (mut i16)))

  (func
    (import "wasm:js-string" "test")
    (param externref)
    (result i32)
  )
  (export "test" (func 0))

  (func
    (import "wasm:js-string" "cast")
    (param externref)
    (result (ref extern))
  )
  (export "cast" (func 1))

  (func
    (import "wasm:js-string" "fromCharCodeArray")
    (param (ref null $arrayMutI16) i32 i32)
    (result (ref extern))
  )
  (export "fromCharCodeArray" (func 2))

  (func
    (import "wasm:js-string" "intoCharCodeArray")
    (param externref (ref null $arrayMutI16) i32)
    (result i32)
  )
  (export "intoCharCodeArray" (func 3))

  (func
    (import "wasm:js-string" "fromCharCode")
    (param i32)
    (result (ref extern))
  )
  (export "fromCharCode" (func 4))

  (func
    (import "wasm:js-string" "fromCodePoint")
    (param i32)
    (result (ref extern))
  )
  (export "fromCodePoint" (func 5))

  (func
    (import "wasm:js-string" "charCodeAt")
    (param externref i32)
    (result i32)
  )
  (export "charCodeAt" (func 6))

  (func
    (import "wasm:js-string" "codePointAt")
    (param externref i32)
    (result i32)
  )
  (export "codePointAt" (func 7))

  (func
    (import "wasm:js-string" "length")
    (param externref)
    (result i32)
  )
  (export "length" (func 8))

  (func
    (import "wasm:js-string" "concat")
    (param externref externref)
    (result (ref extern))
  )
  (export "concat" (func 9))

  (func
    (import "wasm:js-string" "substring")
    (param externref i32 i32)
    (result (ref extern))
  )
  (export "substring" (func 10))

  (func
    (import "wasm:js-string" "equals")
    (param externref externref)
    (result i32)
  )
  (export "equals" (func 11))

  (func
    (import "wasm:js-string" "compare")
    (param externref externref)
    (result i32)
  )
  (export "compare" (func 12))
)`;

let {
  createArrayMutI16,
  arrayLength,
  arraySet,
  arrayGet
} = wasmEvalText(`(module
  (type $arrayMutI16 (array (mut i16)))
  (func (export "createArrayMutI16") (param i32) (result anyref)
    i32.const 0
    local.get 0
    array.new $arrayMutI16
  )
  (func (export "arrayLength") (param arrayref) (result i32)
    local.get 0
    array.len
  )
  (func (export "arraySet") (param (ref $arrayMutI16) i32 i32)
    local.get 0
    local.get 1
    local.get 2
    array.set $arrayMutI16
  )
  (func (export "arrayGet") (param (ref $arrayMutI16) i32) (result i32)
    local.get 0
    local.get 1
    array.get_u $arrayMutI16
  )
)`).exports;

function throwIfNotString(a) {
  if (typeof a !== "string") {
    throw new WebAssembly.RuntimeError();
  }
}
let polyFillImports = {
  test: (string) => {
    if (string === null ||
        typeof string !== "string") {
      return 0;
    }
    return 1;
  },
  cast: (string) => {
    if (string === null ||
        typeof string !== "string") {
      throw new WebAssembly.RuntimeError();
    }
    return string;
  },
  fromCharCodeArray: (array, arrayStart, arrayCount) => {
    arrayStart >>>= 0;
    arrayCount >>>= 0;
    let length = arrayLength(array);
    if (BigInt(arrayStart) + BigInt(arrayCount) > BigInt(length)) {
      throw new WebAssembly.RuntimeError();
    }
    let result = '';
    for (let i = arrayStart; i < arrayStart + arrayCount; i++) {
      result += String.fromCharCode(arrayGet(array, i));
    }
    return result;
  },
  intoCharCodeArray: (string, arr, arrayStart) => {
    arrayStart >>>= 0;
    throwIfNotString(string);
    let arrLength = arrayLength(arr);
    let stringLength = string.length;
    if (BigInt(arrayStart) + BigInt(stringLength) > BigInt(arrLength)) {
      throw new WebAssembly.RuntimeError();
    }
    for (let i = 0; i < stringLength; i++) {
      arraySet(arr, arrayStart + i, string[i].charCodeAt(0));
    }
    return stringLength;
  },
  fromCharCode: (charCode) => {
    charCode >>>= 0;
    return String.fromCharCode(charCode);
  },
  fromCodePoint: (codePoint) => {
    codePoint >>>= 0;
    return String.fromCodePoint(codePoint);
  },
  charCodeAt: (string, stringIndex) => {
    stringIndex >>>= 0;
    throwIfNotString(string);
    if (stringIndex >= string.length)
      throw new WebAssembly.RuntimeError();
    return string.charCodeAt(stringIndex);
  },
  codePointAt: (string, stringIndex) => {
    stringIndex >>>= 0;
    throwIfNotString(string);
    if (stringIndex >= string.length)
      throw new WebAssembly.RuntimeError();
    return string.codePointAt(stringIndex);
  },
  length: (string) => {
    throwIfNotString(string);
    return string.length;
  },
  concat: (stringA, stringB) => {
    throwIfNotString(stringA);
    throwIfNotString(stringB);
    return stringA + stringB;
  },
  substring: (string, startIndex, endIndex) => {
    startIndex >>>= 0;
    endIndex >>>= 0;
    throwIfNotString(string);
    if (startIndex > string.length,
        endIndex > string.length,
        endIndex < startIndex) {
      return "";
    }
    return string.substring(startIndex, endIndex);
  },
  equals: (stringA, stringB) => {
    throwIfNotString(stringA);
    throwIfNotString(stringB);
    return stringA === stringB;
  },
  compare: (stringA, stringB) => {
    throwIfNotString(stringA);
    throwIfNotString(stringB);
    if (stringA < stringB) {
      return -1;
    }
    return stringA === stringB ? 0 : 1;
  },
};

function assertSameBehavior(funcA, funcB, ...params) {
  let resultA;
  let errA = null;
  try {
    resultA = funcA(...params);
  } catch (err) {
    errA = err;
  }

  let resultB;
  let errB = null;
  try {
    resultB = funcB(...params);
  } catch (err) {
    errB = err;
  }

  if (errA || errB) {
    assertEq(errA === null, errB === null, errA ? errA.message : errB.message);
    assertEq(Object.getPrototypeOf(errA), Object.getPrototypeOf(errB));
  }
  assertEq(resultA, resultB);

  if (errA) {
    throw errA;
  }
  return resultA;
}

let builtinExports = wasmEvalText(testModule, {}, {builtins: ["js-string"]}).exports;
let polyfillExports = wasmEvalText(testModule, { 'wasm:js-string': polyFillImports }).exports;

let testStrings = ["", "a", "1", "ab", "hello, world", "\n", "☺", "☺smiley", String.fromCodePoint(0x10000, 0x10001)];
let testCharCodes = [1, 2, 3, 10, 0x7f, 0xff, 0xfffe, 0xffff];
let testCodePoints = [1, 2, 3, 10, 0x7f, 0xff, 0xfffe, 0xffff, 0x10000, 0x10001];

for (let a of WasmExternrefValues) {
  assertSameBehavior(
    builtinExports['test'],
    polyfillExports['test'],
    a
  );
  try {
    assertSameBehavior(
      builtinExports['cast'],
      polyfillExports['cast'],
      a
    );
  } catch (err) {
    assertEq(err instanceof WebAssembly.RuntimeError, true);
  }
}

for (let a of testCharCodes) {
  assertSameBehavior(
    builtinExports['fromCharCode'],
    polyfillExports['fromCharCode'],
    a
  );
}

for (let a of testCodePoints) {
  assertSameBehavior(
    builtinExports['fromCodePoint'],
    polyfillExports['fromCodePoint'],
    a
  );
}

for (let a of testStrings) {
  let length = assertSameBehavior(
    builtinExports['length'],
    polyfillExports['length'],
    a
  );

  for (let i = 0; i < length; i++) {
    let charCode = assertSameBehavior(
      builtinExports['charCodeAt'],
      polyfillExports['charCodeAt'],
      a, i
    );
  }
  for (let i = 0; i < length; i++) {
    let charCode = assertSameBehavior(
      builtinExports['codePointAt'],
      polyfillExports['codePointAt'],
      a, i
    );
  }

  let arrayMutI16 = createArrayMutI16(length);
  assertSameBehavior(
    builtinExports['intoCharCodeArray'],
    polyfillExports['intoCharCodeArray'],
    a, arrayMutI16, 0
  );
  assertSameBehavior(
    builtinExports['fromCharCodeArray'],
    polyfillExports['fromCharCodeArray'],
    arrayMutI16, 0, length
  );

  for (let i = 0; i < length; i++) {
    for (let j = 0; j < length; j++) {
      assertSameBehavior(
        builtinExports['substring'],
        polyfillExports['substring'],
        a, i, j
      );
    }
  }
}

for (let a of testStrings) {
  for (let b of testStrings) {
    assertSameBehavior(
      builtinExports['concat'],
      polyfillExports['concat'],
      a, b
    );
    assertSameBehavior(
      builtinExports['equals'],
      polyfillExports['equals'],
      a, b
    );
    assertSameBehavior(
      builtinExports['compare'],
      polyfillExports['compare'],
      a, b
    );
  }
}

// fromCharCodeArray length is an unsigned integer
{
  let arrayMutI16 = createArrayMutI16(1);
  assertErrorMessage(() => assertSameBehavior(
    builtinExports['fromCharCodeArray'],
    polyfillExports['fromCharCodeArray'],
    arrayMutI16, 1, -1
  ), WebAssembly.RuntimeError, /./);
}
