// This file essentially reproduces an example Angular component to map testing,
// among other typescript edge cases.

import def, { decoratorFactory } from './src/mod.ts';

import * as ns from './src/mod.ts';

@decoratorFactory({
  selector: 'app-root',
})
export class AppComponent {
  title = 'app';
}

const fn = arg => {
  console.log("here");
};
fn("arg");

// Un-decorated exported classes present a mapping challege because
// the class name is mapped to an unhelpful export assignment.
export class ExportedOther {
  title = 'app';
}

class AnotherThing {
  prop = 4;
}

const ExpressionClass = class Foo {
  prop = 4;
};

class SubDecl extends AnotherThing {
  prop = 4;
}

let SubVar = class SubExpr extends AnotherThing {
  prop = 4;
};

ns;

export default function(){
  // This file is specifically for testing the mappings of classes and things
  // above, which means we don't want to include _other_ references to then.
  // To avoid having them be optimized out, we include a no-op eval.
  eval("");

  console.log("pause here");
}
