// |jit-test| skip-if: !wasmFunctionReferencesEnabled()

// Generates combinations of different block types and operations for
// non-defaultable locals (local.set / .tee / .get).
// See the function references specification on the updated algorithm
// for validating non-null references in locals.

const KINDS = [
  "block",
  "loop",
  "try",
  "catch",
  "if",
  "else",
]
const INITIALIZED = [
  "nowhere",
  "outer",
  "inner",
  "outer-tee",
  "inner-tee",
];
const USED = [
  "outer",
  "inner",
  "after-inner",
  "after-outer",
];

function generateBlock(kind, contents) {
  switch (kind) {
    case "block": {
      return `block\n${contents}end\n`
    }
    case "loop": {
      return `loop\n${contents}end\n`
    }
    case "try": {
      return `try\n${contents}end\n`
    }
    case "catch": {
      return `try\ncatch_all\n${contents}end\n`
    }
    case "if": {
      return `i32.const 0\nif\n${contents}end\n`
    }
    case "else": {
      return `i32.const 0\nif\nelse\n${contents}end\n`
    }
  }
}

// Generate a variation of the module below:
//
// (func
//  (block
//    $outer
//    (block
//      $inner
//    )
//    $after-inner
//  )
//  $after-outer
// )
//
// Where a local is used and initialized at different points depending on the
// parameters. The block kinds of the inner and outer block may also be
// customized.
function generateModule(outerBlockKind, innerBlockKind, initializedWhere, usedWhere) {
  const INITIALIZE_STMT = '(local.set 0 ref.func 0)\n';
  const INITIALIZE_STMT2 = '(drop (local.tee 0 ref.func 0))\n';
  const USE_STMT = '(drop local.get 0)\n';

  // inner block
  let innerBlockContents = '';
  if (initializedWhere === 'inner') {
    innerBlockContents += INITIALIZE_STMT;
  } else if (initializedWhere === 'inner-tee') {
    innerBlockContents += INITIALIZE_STMT2;
  }
  if (usedWhere === 'inner') {
    innerBlockContents += USE_STMT;
  }
  let innerBlock = generateBlock(innerBlockKind, innerBlockContents);

  // outer block
  let outerBlockContents = '';
  if (initializedWhere === 'outer') {
    outerBlockContents += INITIALIZE_STMT;
  } else if (initializedWhere === 'outer-tee') {
    outerBlockContents += INITIALIZE_STMT2;
  }
  if (usedWhere === 'outer') {
    outerBlockContents += USE_STMT;
  }
  outerBlockContents += innerBlock;
  if (usedWhere === 'after-inner') {
    outerBlockContents += USE_STMT;
  }
  let outerBlock = generateBlock(outerBlockKind, outerBlockContents);

  // after outer block
  let afterOuterBlock = '';
  if (usedWhere === 'after-outer') {
    afterOuterBlock += USE_STMT;
  }

  return `(module
  (type $t (func))
  (func (export "test")
    (local (ref $t))
${outerBlock}${afterOuterBlock}  )
)`;
}

const LOGGING = false;

for (let outer of KINDS) {
  for (let inner of KINDS) {
    for (let initialized of INITIALIZED) {
      for (let used of USED) {
        let text = generateModule(outer, inner, initialized, used);

        let expectPass;
        switch (initialized) {
          case "outer":
          case "outer-tee": {
            // Defining the local in the outer block makes it valid
            // in the outer block, the inner block, and after the
            // inner block
            expectPass = used !== "after-outer";
            break;
          }
          case "inner":
          case "inner-tee": {
            // Defining the local in the inner block makes it valid
            // in the inner block
            //
            // NOTE: an extension to typing could make this valid
            // after the inner block in some cases
            expectPass = used === "inner";
            break;
          }
          case "nowhere": {
            // Not defining the local makes it always invalid to
            // use
            expectPass = false;
            break;
          }
        }

        if (LOGGING) {
          console.log();
          console.log(`TEST: outer=${outer}, inner=${inner}, initialized=${initialized}, used=${used}`);
          console.log(expectPass ? "EXPECT PASS" : "EXPECT FAIL");
          console.log(text);
        }

        let binary = wasmTextToBinary(text);
        assertEq(WebAssembly.validate(binary), expectPass);
        if (!expectPass) {
          // Check if the error message is right.
          try {
            new WebAssembly.Module(binary);
          } catch (ex) {
            assertEq(true, /local\.get read from unset local/.test(ex.message));
          }
        }
      }
    }
  }
}
