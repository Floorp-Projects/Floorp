// |reftest| skip-if(!xulRuntime.shell)

function importDecl(specifiers, source) {
  return {
    type: "ImportDeclaration",
    specifiers,
    source,
  };
}

function importSpec(id, name) {
  return {
    type: "ImportSpecifier",
    id,
    name,
  };
}

function exportDecl(declaration, specifiers, source, isDefault) {
  return {
    type: "ExportDeclaration",
    declaration,
    specifiers,
    source,
    isDefault,
  };
}

function exportSpec(id, name) {
  return {
    type: "ExportSpecifier",
    id,
    name,
  };
}

function exportNamespaceSpec(name) {
  return {
    type: "ExportNamespaceSpecifier",
    name,
  };
}

function assertModule(src, patt) {
  program(patt).assert(Reflect.parse(src, {target: "module"}));
}

assertModule(`
  import {"x" as y} from "module";
`, [
  importDecl([importSpec(literal("x"), ident("y"))], literal("module")),
]);

assertModule(`
  var x;
  export {x as "y"};
`, [
  varDecl([{id: ident("x"), init: null}]),
  exportDecl(null, [exportSpec(ident("x"), literal("y"))], null, false),
]);

assertModule(`
  export {x as "y"} from "module";
`, [
  exportDecl(null, [exportSpec(ident("x"), literal("y"))], literal("module"), false),
]);

assertModule(`
  export {"x" as y} from "module";
`, [
  exportDecl(null, [exportSpec(literal("x"), ident("y"))], literal("module"), false),
]);

assertModule(`
  export {"x" as "y"} from "module";
`, [
  exportDecl(null, [exportSpec(literal("x"), literal("y"))], literal("module"), false),
]);

assertModule(`
  export {"x"} from "module";
`, [
  exportDecl(null, [exportSpec(literal("x"), literal("x"))], literal("module"), false),
]);

assertModule(`
  export * as "x" from "module";
`, [
  exportDecl(null, [exportNamespaceSpec(literal("x"))], literal("module"), false),
]);

if (typeof reportCompare === "function")
  reportCompare(true, true);
