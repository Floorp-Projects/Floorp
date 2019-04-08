
// TSEnumDeclaration
enum Color {
  // TSEnumMember
  Red,
  Green,
  Blue,
}

class Example<T> {
  // TSParameterProperty
  constructor(public foo) {

  }

  method(): never {
    throw new Error();
  }
}

// TSTypeAssertion
var foo = <any>window;

// TSAsExpression
(window as any);

// TSNonNullExpression
(window!);

// TSModuleDeclaration
namespace TheSpace {
  function fn() {

  }
}

// TSImportEqualsDeclaration
import ImportedClass = require("mod");

// TSExportAssignment
export = Example;
