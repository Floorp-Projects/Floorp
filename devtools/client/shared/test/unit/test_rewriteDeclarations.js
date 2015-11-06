/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
Cu.import("resource://devtools/shared/Loader.jsm");
const {parseDeclarations, RuleRewriter} =
      devtools.require("devtools/client/shared/css-parsing-utils");

const TEST_DATA = [
  {
    desc: "simple set",
    input: "p:v;",
    instruction: {type: "set", name: "p", value: "N", priority: "",
                  index: 0},
    expected: "p:N;"
  },
  {
    desc: "simple set clearing !important",
    input: "p:v !important;",
    instruction: {type: "set", name: "p", value: "N", priority: "",
                  index: 0},
    expected: "p:N;"
  },
  {
    desc: "simple set adding !important",
    input: "p:v;",
    instruction: {type: "set", name: "p", value: "N", priority: "important",
                  index: 0},
    expected: "p:N !important;"
  },
  {
    desc: "simple set between comments",
    input: "/*color:red;*/ p:v; /*color:green;*/",
    instruction: {type: "set", name: "p", value: "N", priority: "",
                  index: 1},
    expected: "/*color:red;*/ p:N; /*color:green;*/"
  },
  // The rule view can generate a "set" with a previously unknown
  // property index; which should work like "create".
  {
    desc: "set at unknown index",
    input: "a:b; e: f;",
    instruction: {type: "set", name: "c", value: "d", priority: "",
                  index: 2},
    expected: "a:b; e: f;c: d;"
  },
  {
    desc: "simple rename",
    input: "p:v;",
    instruction: {type: "rename", name: "p", newName: "q", index: 0},
    expected: "q:v;"
  },
  // "rename" is passed the name that the user entered, and must do
  // any escaping necessary to ensure that this is an identifier.
  {
    desc: "rename requiring escape",
    input: "p:v;",
    instruction: {type: "rename", name: "p", newName: "a b", index: 0},
    expected: "a\\ b:v;"
  },
  {
    desc: "simple create",
    input: "",
    instruction: {type: "create", name: "p", value: "v", priority: "important",
                  index: 0},
    expected: "p: v !important;"
  },
  {
    desc: "create between two properties",
    input: "a:b; e: f;",
    instruction: {type: "create", name: "c", value: "d", priority: "",
                  index: 1},
    expected: "a:b; c: d;e: f;"
  },
  // "create" is passed the name that the user entered, and must do
  // any escaping necessary to ensure that this is an identifier.
  {
    desc: "create requiring escape",
    input: "",
    instruction: {type: "create", name: "a b", value: "d", priority: "",
                  index: 1},
    expected: "a\\ b: d;"
  },
  {
    desc: "simple disable",
    input: "p:v;",
    instruction: {type: "enable", name: "p", value: false, index: 0},
    expected: "/*! p:v; */"
  },
  {
    desc: "simple enable",
    input: "/* color:v; */",
    instruction: {type: "enable", name: "color", value: true, index: 0},
    expected: "color:v;"
  },
  {
    desc: "enable with following property in comment",
    input: "/* color:red; color: blue; */",
    instruction: {type: "enable", name: "color", value: true, index: 0},
    expected: "color:red; /* color: blue; */"
  },
  {
    desc: "enable with preceding property in comment",
    input: "/* color:red; color: blue; */",
    instruction: {type: "enable", name: "color", value: true, index: 1},
    expected: "/* color:red; */ color: blue;"
  },
  {
    desc: "simple remove",
    input: "a:b;c:d;e:f;",
    instruction: {type: "remove", name: "c", index: 1},
    expected: "a:b;e:f;"
  },
  {
    desc: "disable with comment ender in string",
    input: "content: '*/';",
    instruction: {type: "enable", name: "content", value: false, index: 0},
    expected: "/*! content: '*\\/'; */"
  },
  {
    desc: "enable with comment ender in string",
    input: "/* content: '*\\/'; */",
    instruction: {type: "enable", name: "content", value: true, index: 0},
    expected: "content: '*/';"
  },
  {
    desc: "enable requiring semicolon insertion",
    // Note the lack of a trailing semicolon in the comment.
    input: "/* color:red */ color: blue;",
    instruction: {type: "enable", name: "color", value: true, index: 0},
    expected: "color:red; color: blue;"
  },
  {
    desc: "create requiring semicolon insertion",
    // Note the lack of a trailing semicolon.
    input: "color: red",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 1},
    expected: "color: red;a: b;"
  },

  // Newline insertion.
  {
    desc: "simple newline insertion",
    input: "\ncolor: red;\n",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 1},
    expected: "\ncolor: red;\na: b;\n"
  },
  // Newline insertion.
  {
    desc: "semicolon insertion before newline",
    // Note the lack of a trailing semicolon.
    input: "\ncolor: red\n",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 1},
    expected: "\ncolor: red;\na: b;\n"
  },
  // Newline insertion.
  {
    desc: "newline and semicolon insertion",
    // Note the lack of a trailing semicolon and newline.
    input: "\ncolor: red",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 1},
    expected: "\ncolor: red;\na: b;\n"
  },

  // Newline insertion and indentation.
  {
    desc: "indentation with create",
    input: "\n  color: red;\n",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 1},
    expected: "\n  color: red;\n  a: b;\n"
  },
  // Newline insertion and indentation.
  {
    desc: "indentation plus semicolon insertion before newline",
    // Note the lack of a trailing semicolon.
    input: "\n  color: red\n",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 1},
    expected: "\n  color: red;\n  a: b;\n"
  },
  {
    desc: "indentation inserted before trailing whitespace",
    // Note the trailing whitespace.  This could come from a rule
    // like:
    // @supports (mumble) {
    //   body {
    //     color: red;
    //   }
    // }
    // Here if we create a rule we don't want it to follow
    // the indentation of the "}".
    input: "\n    color: red;\n  ",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 1},
    expected: "\n    color: red;\n    a: b;\n  "
  },
  // Newline insertion and indentation.
  {
    desc: "indentation comes from preceding comment",
    // Note how the comment comes before the declaration.
    input: "\n  /* comment */ color: red\n",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 1},
    expected: "\n  /* comment */ color: red;\n  a: b;\n"
  },
  // Default indentation.
  {
    desc: "use of default indentation",
    input: "\n",
    instruction: {type: "create", name: "a", value: "b", priority: "",
                  index: 0},
    expected: "\n\ta: b;\n"
  },

  // Deletion handles newlines properly.
  {
    desc: "deletion removes newline",
    input: "a:b;\nc:d;\ne:f;",
    instruction: {type: "remove", name: "c", index: 1},
    expected: "a:b;\ne:f;"
  },
  // Deletion handles newlines properly.
  {
    desc: "deletion remove blank line",
    input: "\n  a:b;\n  c:d;  \ne:f;",
    instruction: {type: "remove", name: "c", index: 1},
    expected: "\n  a:b;\ne:f;"
  },
  // Deletion handles newlines properly.
  {
    desc: "deletion leaves comment",
    input: "\n  a:b;\n  /* something */ c:d;  \ne:f;",
    instruction: {type: "remove", name: "c", index: 1},
    expected: "\n  a:b;\n  /* something */   \ne:f;"
  },
  // Deletion handles newlines properly.
  {
    desc: "deletion leaves previous newline",
    input: "\n  a:b;\n  c:d;  \ne:f;",
    instruction: {type: "remove", name: "e", index: 2},
    expected: "\n  a:b;\n  c:d;  \n"
  },
  // Deletion handles newlines properly.
  {
    desc: "deletion removes trailing whitespace",
    input: "\n  a:b;\n  c:d;  \n    e:f;",
    instruction: {type: "remove", name: "e", index: 2},
    expected: "\n  a:b;\n  c:d;  \n"
  },
  // Deletion handles newlines properly.
  {
    desc: "deletion preserves indentation",
    input: "  a:b;\n  c:d;  \n    e:f;",
    instruction: {type: "remove", name: "a", index: 0},
    expected: "  c:d;  \n    e:f;"
  },

  // Termination insertion corner case.
  {
    desc: "enable single quote termination",
    input: "/* content: 'hi */ color: red;",
    instruction: {type: "enable", name: "content", value: true, index: 0},
    expected: "content: 'hi'; color: red;",
    changed: {0: "'hi'"}
  },
  // Termination insertion corner case.
  {
    desc: "create single quote termination",
    input: "content: 'hi",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "content: 'hi';color: red;",
    changed: {0: "'hi'"}
  },

  // Termination insertion corner case.
  {
    desc: "enable double quote termination",
    input: "/* content: \"hi */ color: red;",
    instruction: {type: "enable", name: "content", value: true, index: 0},
    expected: "content: \"hi\"; color: red;",
    changed: {0: "\"hi\""}
  },
  // Termination insertion corner case.
  {
    desc: "create double quote termination",
    input: "content: \"hi",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "content: \"hi\";color: red;",
    changed: {0: "\"hi\""}
  },

  // Termination insertion corner case.
  {
    desc: "enable url termination",
    input: "/* background-image: url(something.jpg */ color: red;",
    instruction: {type: "enable", name: "background-image", value: true,
                  index: 0},
    expected: "background-image: url(something.jpg); color: red;",
    changed: {0: "url(something.jpg)"}
  },
  // Termination insertion corner case.
  {
    desc: "create url termination",
    input: "background-image: url(something.jpg",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "background-image: url(something.jpg);color: red;",
    changed: {0: "url(something.jpg)"}
  },

  // Termination insertion corner case.
  {
    desc: "enable url single quote termination",
    input: "/* background-image: url('something.jpg */ color: red;",
    instruction: {type: "enable", name: "background-image", value: true,
                  index: 0},
    expected: "background-image: url('something.jpg'); color: red;",
    changed: {0: "url('something.jpg')"}
  },
  // Termination insertion corner case.
  {
    desc: "create url single quote termination",
    input: "background-image: url('something.jpg",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "background-image: url('something.jpg');color: red;",
    changed: {0: "url('something.jpg')"}
  },

  // Termination insertion corner case.
  {
    desc: "create url double quote termination",
    input: "/* background-image: url(\"something.jpg */ color: red;",
    instruction: {type: "enable", name: "background-image", value: true,
                  index: 0},
    expected: "background-image: url(\"something.jpg\"); color: red;",
    changed: {0: "url(\"something.jpg\")"}
  },
  // Termination insertion corner case.
  {
    desc: "enable url double quote termination",
    input: "background-image: url(\"something.jpg",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "background-image: url(\"something.jpg\");color: red;",
    changed: {0: "url(\"something.jpg\")"}
  },

  // Termination insertion corner case.
  {
    desc: "create backslash termination",
    input: "something: \\",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "something: \\\\;color: red;",
    // The lexer rewrites the token before we see it.  However this is
    // so obscure as to be inconsequential.
    changed: {0: "\uFFFD\\"}
  },

  // Termination insertion corner case.
  {
    desc: "enable backslash single quote termination",
    input: "something: '\\",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "something: '\\\\';color: red;",
    changed: {0: "'\\\\'"}
  },
  {
    desc: "enable backslash double quote termination",
    input: "something: \"\\",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "something: \"\\\\\";color: red;",
    changed: {0: "\"\\\\\""}
  },

  // Termination insertion corner case.
  {
    desc: "enable comment termination",
    input: "something: blah /* comment ",
    instruction: {type: "create", name: "color", value: "red", priority: "",
                  index: 1},
    expected: "something: blah /* comment*/; color: red;"
  },

  // Rewrite a "heuristic override" comment.
  {
    desc: "enable with heuristic override comment",
    input: "/*! walrus: zebra; */",
    instruction: {type: "enable", name: "walrus", value: true, index: 0},
    expected: "walrus: zebra;"
  },

  // Sanitize a bad value.
  {
    desc: "create sanitize unpaired brace",
    input: "",
    instruction: {type: "create", name: "p", value: "}", priority: "",
                  index: 0},
    expected: "p: \\};",
    changed: {0: "\\}"}
  },
  // Sanitize a bad value.
  {
    desc: "set sanitize unpaired brace",
    input: "walrus: zebra;",
    instruction: {type: "set", name: "walrus", value: "{{}}}", priority: "",
                  index: 0},
    expected: "walrus: {{}}\\};",
    changed: {0: "{{}}\\}"}
  },
  // Sanitize a bad value.
  {
    desc: "enable sanitize unpaired brace",
    input: "/*! walrus: }*/",
    instruction: {type: "enable", name: "walrus", value: true, index: 0},
    expected: "walrus: \\};",
    changed: {0: "\\}"}
  },

  // Creating a new declaration does not require an attempt to
  // terminate a previous commented declaration.
  {
    desc: "disabled declaration does not need semicolon insertion",
    input: "/*! no: semicolon */\n",
    instruction: {type: "create", name: "walrus", value: "zebra", priority: "",
                  index: 1},
    expected: "/*! no: semicolon */\nwalrus: zebra;\n",
    changed: {}
  },
];

function rewriteDeclarations(inputString, instruction, defaultIndentation) {
  let rewriter = new RuleRewriter(null, inputString);
  rewriter.defaultIndentation = defaultIndentation;

  switch (instruction.type) {
    case "rename":
      rewriter.renameProperty(instruction.index, instruction.name,
                              instruction.newName);
      break;

    case "enable":
      rewriter.setPropertyEnabled(instruction.index, instruction.name,
                                  instruction.value);
      break;

    case "create":
      rewriter.createProperty(instruction.index, instruction.name,
                              instruction.value, instruction.priority);
      break;

    case "set":
      rewriter.setProperty(instruction.index, instruction.name,
                           instruction.value, instruction.priority);
      break;

    case "remove":
      rewriter.removeProperty(instruction.index, instruction.name);
      break;

    default:
      throw new Error("unrecognized instruction");
  }

  return rewriter.getResult();
}

function run_test() {
  let i = 0;
  for (let test of TEST_DATA) {
    ++i;
    let {changed, text} = rewriteDeclarations(test.input, test.instruction,
                                              "\t");
    equal(text, test.expected, "output for " + test.desc);

    let expectChanged;
    if ("changed" in test) {
      expectChanged = test.changed;
    } else {
      expectChanged = {};
    }
    deepEqual(changed, expectChanged, "changed result for " + test.desc);
  }
}
