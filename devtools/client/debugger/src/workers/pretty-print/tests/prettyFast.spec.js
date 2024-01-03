/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Copyright 2013 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE.md or:
 * http://opensource.org/licenses/BSD-2-Clause
 */
import { prettyFast } from "../pretty-fast";
import { SourceMapConsumer } from "devtools/client/shared/vendor/source-map/source-map";

const cases = [
  {
    name: "Simple function",
    input: "function foo() { bar(); }",
  },
  {
    name: "Nested function",
    input: "function foo() { function bar() { debugger; } bar(); }",
  },
  {
    name: "Immediately invoked function expression",
    input: "(function(){thingy()}())",
  },
  {
    name: "Single line comment",
    input: `
    // Comment
    function foo() { bar(); }`,
  },
  {
    name: "Multi line comment",
    input: `
    /* Comment
    more comment */
    function foo() { bar(); }
    `,
  },
  { name: "Null assignment", input: "var i=null;" },
  { name: "Undefined assignment", input: "var i=undefined;" },
  { name: "Void 0 assignment", input: "var i=void 0;" },
  {
    name: "This property access",
    input: "var foo=this.foo;\n",
  },

  {
    name: "True assignment",
    input: "var foo=true;\n",
  },

  {
    name: "False assignment",
    input: "var foo=false;\n",
  },

  {
    name: "For loop",
    input: "for (var i = 0; i < n; i++) { console.log(i); }",
  },

  {
    name: "for..of loop",
    input: "for (const x of [1,2,3]) { console.log(x) }",
  },

  {
    name: "String with semicolon",
    input: "var foo = ';';\n",
  },

  {
    name: "String with quote",
    input: 'var foo = "\'";\n',
  },

  {
    name: "Function calls",
    input: "var result=func(a,b,c,d);",
  },

  {
    name: "Regexp",
    input: "var r=/foobar/g;",
  },

  {
    name: "In operator",
    input: "if(foo in bar){doThing()}",
    output: "if (foo in bar) {\n" + "  doThing()\n" + "}\n",
  },
  {
    name: "With statement",
    input: "with(obj){crock()}",
  },
  {
    name: "New expression",
    input: "var foo=new Foo();",
  },
  {
    name: "Continue/break statements",
    input: "while(1){if(x){continue}if(y){break}if(z){break foo}}",
  },
  {
    name: "Instanceof",
    input: "var a=x instanceof y;",
  },
  {
    name: "Binary operators",
    input: "var a=5*30;var b=5>>3;",
  },
  {
    name: "Delete",
    input: "delete obj.prop;",
  },

  {
    name: "Try/catch/finally statement",
    input: "try{dangerous()}catch(e){handle(e)}finally{cleanup()}",
  },
  {
    name: "If/else statement",
    input: "if(c){then()}else{other()}",
  },
  {
    name: "If/else without curlies",
    input: "if(c) a else b",
  },
  {
    name: "Objects",
    input: `
      var o={a:1,
      b:2};`,
  },
  {
    name: "Do/while loop",
    input: "do{x}while(y)",
  },
  {
    name: "Arrays",
    input: "var a=[1,2,3];",
  },
  {
    name: "Arrays and spread operator",
    input: "var a=[1,...[2,3],...[], 4];",
  },
  {
    name: "Empty object/array literals",
    input: `let a=[];const b={};c={...{},d: 42};for(let x of []){for(let y in {}){}}`,
  },
  {
    name: "Code that relies on ASI",
    input: `
           var foo = 10
           var bar = 20
           function g() {
             a()
             b()
           }`,
  },
  {
    name: "Ternary operator",
    input: "bar?baz:bang;",
  },
  {
    name: "Switch statements",
    input: "switch(x){case a:foo();break;default:bar()}",
  },

  {
    name: "Multiple single line comments",
    input: `function f() {
             // a
             // b
             // c
           }`,
  },
  {
    name: "Indented multiline comment",
    input: `function foo() {
             /**
              * java doc style comment
              * more comment
              */
             bar();
           }`,
  },
  {
    name: "ASI return",
    input: `function f() {
             return
             {}
           }`,
  },
  {
    name: "Non-ASI property access",
    input: `[1,2,3]
           [0]`,
  },
  {
    name: "Non-ASI in",
    input: `'x'
           in foo`,
  },

  {
    name: "Non-ASI function call",
    input: `f
           ()`,
  },
  {
    name: "Non-ASI new",
    input: `new
           F()`,
  },
  {
    name: "Getter and setter literals",
    input: "var obj={get foo(){return this._foo},set foo(v){this._foo=v}}",
  },
  {
    name: "Escaping backslashes in strings",
    input: "'\\\\'\n",
  },
  {
    name: "Escaping carriage return in strings",
    input: "'\\r'\n",
  },
  {
    name: "Escaping tab in strings",
    input: "'\\t'\n",
  },
  {
    name: "Escaping vertical tab in strings",
    input: "'\\v'\n",
  },
  {
    name: "Escaping form feed in strings",
    input: "'\\f'\n",
  },
  {
    name: "Escaping null character in strings",
    input: "'\\0'\n",
  },
  {
    name: "Bug 977082 - space between grouping operator and dot notation",
    input: `JSON.stringify(3).length;
           ([1,2,3]).length;
           (new Date()).toLocaleString();`,
  },
  {
    name: "Bug 975477 don't move end of line comments to next line",
    input: `switch (request.action) {
             case 'show': //$NON-NLS-0$
               if (localStorage.hideicon !== 'true') { //$NON-NLS-0$
                 chrome.pageAction.show(sender.tab.id);
               }
               break;
             case 'hide': /*Multiline
               Comment */
               break;
             default:
               console.warn('unknown request'); //$NON-NLS-0$
               // don't respond if you don't understand the message.
               return;
           }`,
  },
  {
    name: "Const handling",
    input: "const d = 'yes';\n",
  },
  {
    name: "Let handling without value",
    input: "let d;\n",
  },
  {
    name: "Let handling with value",
    input: "let d = 'yes';\n",
  },
  {
    name: "Template literals",
    // issue in acorn
    input: "`abc${JSON.stringify({clas: 'testing'})}def`;{a();}",
  },
  {
    name: "Class handling",
    input: "class  Class{constructor(){}}",
  },
  {
    name: "Subclass handling",
    input: "class  Class  extends  Base{constructor(){}}",
  },
  {
    name: "Unnamed class handling",
    input: "let unnamed=class{constructor(){}}",
  },
  {
    name: "Named class handling",
    input: "let unnamed=class Class{constructor(){}}",
  },
  {
    name: "Class extension within a function",
    input: "(function() {  class X extends Y { constructor() {} }  })()",
  },
  {
    name: "Bug 1261971 - indentation after switch statement",
    input: "{switch(x){}if(y){}done();}",
  },
  {
    name: "Bug 1206633 - spaces in for of",
    input: "for (let tab of tabs) {}",
  },
  {
    name: "Bug pretty-sure-3 - escaping line and paragraph separators",
    input: "x = '\\u2029\\u2028';",
  },
  {
    name: "Bug pretty-sure-4 - escaping null character before digit",
    input: "x = '\\u00001';",
  },
  {
    name: "Bug pretty-sure-5 - empty multiline comment shouldn't throw exception",
    input: `{
           /*
           */
             return;
           }`,
  },
  {
    name: "Bug pretty-sure-6 - inline comment shouldn't move parenthesis to next line",
    input: `return /* inline comment */ (
             1+1);`,
  },
  {
    name: "Bug pretty-sure-7 - accessing a literal number property requires a space",
    input: "0..toString()+x.toString();",
  },
  {
    name: "Bug pretty-sure-8 - return and yield only accept arguments when on the same line",
    input: `{
             return
             (x)
             yield
             (x)
             yield
             *x
           }`,
  },
  {
    name: "Bug pretty-sure-9 - accept unary operator at start of file",
    input: "+ 0",
  },
  {
    name: "Stack-keyword property access",
    input: "foo.a=1.1;foo.do.switch.case.default=2.2;foo.b=3.3;\n",
  },
  {
    name: "Dot handling with let which is identifier name",
    input: "y.let.let = 1.23;\n",
  },
  {
    name: "Dot handling with keywords which are identifier name",
    input: "y.await.break.const.delete.else.return.new.yield = 1.23;\n",
  },
  {
    name: "Optional chaining parsing support",
    input: "x?.y?.z?.['a']?.check();\n",
  },
  {
    name: "Private fields parsing support",
    input: `
      class MyClass {
        constructor(a) {
          this.#a = a;this.#b = Math.random();this.ab = this.#getAB();
        }
        #a
        #b = "default value"
        static #someStaticPrivate
        #getA() {
          return this.#a;
        }
        #getAB() {
          return this.#getA()+this.
            #b
        }
      }
    `,
  },
  {
    name: "Long parenthesis",
    input: `
      if (thisIsAVeryLongVariable && thisIsAnotherOne || yetAnotherVeryLongVariable) {
        (thisIsAnotherOne = thisMayReturnNull() || "hi", thisIsAVeryLongVariable = 42, yetAnotherVeryLongVariable && doSomething(true /* do it well */,thisIsAVeryLongVariable, thisIsAnotherOne, yetAnotherVeryLongVariable))
      }
      for (let thisIsAnotherVeryLongVariable = 0; i < thisIsAnotherVeryLongVariable.length; thisIsAnotherVeryLongVariable++) {}
      const x = ({thisIsAnotherVeryLongPropertyName: "but should not cause the paren to be a line delimiter"})
    `,
  },
  {
    name: "Fat arrow function",
    input: `
      const a = () => 42
      addEventListener("click", e => { return false });
      const sum = (c,d) => c+d
    `,
  },
];

const includesOnly = cases.find(({ only }) => only);

for (const { name, input, only, skip } of cases) {
  if ((includesOnly && !only) || skip) {
    continue;
  }
  // Wrapping name into a string to avoid jest/valid-title failures
  test(`${name}`, async () => {
    const actual = prettyFast(input, {
      indent: "  ",
      url: "test.js",
    });

    expect(actual.code).toMatchSnapshot();

    const smc = await new SourceMapConsumer(actual.map.toJSON());
    const mappings = [];
    smc.eachMapping(
      ({ generatedColumn, generatedLine, originalColumn, originalLine }) => {
        mappings.push(
          `(${originalLine}, ${originalColumn}) -> (${generatedLine}, ${generatedColumn})`
        );
      }
    );
    expect(mappings).toMatchSnapshot();
  });
}
