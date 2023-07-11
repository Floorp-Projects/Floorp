// Verify the environment chain for Debugger.Frame described in
// js/src/vm/EnvironmentObject.h.

const g = newGlobal({ newCompartment: true });
const dbg = Debugger(g);

dbg.onEnterFrame = frame => {
  if (frame.script.displayName !== "target") {
    return;
  }
  dbg.onEnterFrame = () => {};

  const bindings = {
    bindings_prop: 50,

    bindings_prop_var: 61,
    bindings_prop_lexical: 71,
    bindings_prop_unqualified: 81,
  };

  const {envs, vars} = JSON.parse(frame.evalWithBindings(`
var qualified = 10;
unqualified = 20;
let lexical = 30;
this.prop = 40;

var bindings_prop_var = 60;
let bindings_prop_lexical = 70;
bindings_prop_unqualified = 80;

const vars = {
  bindings_prop_var,
  bindings_prop_lexical,
  bindings_prop_unqualified,
};

const envs = [];
let env = getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: getEnvironmentObjectType(env) || "*global*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "qualified"),
    unqualified: !!Object.getOwnPropertyDescriptor(env, "unqualified"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "lexical"),
    prop: !!Object.getOwnPropertyDescriptor(env, "prop"),
    bindings_prop: !!Object.getOwnPropertyDescriptor(env, "bindings_prop"),

    bindings_prop_var: !!Object.getOwnPropertyDescriptor(env, "bindings_prop_var"),
    bindings_prop_var_value: Object.getOwnPropertyDescriptor(env, "bindings_prop_var")?.value,
    bindings_prop_lexical: !!Object.getOwnPropertyDescriptor(env, "bindings_prop_lexical"),
    bindings_prop_lexical_value: Object.getOwnPropertyDescriptor(env, "bindings_prop_lexical")?.value,
    bindings_prop_unqualified: !!Object.getOwnPropertyDescriptor(env, "bindings_prop_unqualified"),
    bindings_prop_unqualified_value: Object.getOwnPropertyDescriptor(env, "bindings_prop_unqualified")?.value,
  });

  env = getEnclosingEnvironmentObject(env);
}
JSON.stringify({envs, vars});
`, bindings).return);

  assertEq(vars.bindings_prop_var, 60,
           "qualified var should read the value set by the declaration");
  assertEq(vars.bindings_prop_lexical, 70,
           "lexical should read the value set by the declaration");
  assertEq(vars.bindings_prop_unqualified, 80,
           "unqualified name should read the value set by the assignment");

  assertEq(bindings.bindings_prop_var, 61,
          "the original bindings property must not be overwritten for var");
  assertEq(bindings.bindings_prop_lexical, 71,
          "the original bindings property must not be overwritten for lexical");
  assertEq(bindings.bindings_prop_unqualified, 81,
          "the original bindings property must not be overwritten for unqualified");

  assertEq(envs.length, 4);

  let i = 0, env;

  // NOTE: lexical is optimized and uses frame slot, even if it conflicts with
  //       bindings object properties.

  env = envs[i]; i++;
  assertEq(env.type, "WithEnvironmentObject");
  assertEq(env.qualified, false);
  assertEq(env.unqualified, false);
  assertEq(env.lexical, false);
  assertEq(env.prop, false);
  assertEq(env.bindings_prop, true, "bindings property must live in the with env for bindings");

  assertEq(env.bindings_prop_var, true,
           "bindings property must live in the with env for bindings");
  assertEq(env.bindings_prop_var_value, 60,
           "bindings property must be overwritten for var");
  assertEq(env.bindings_prop_lexical, true,
           "bindings property must live in the with env for bindings");
  assertEq(env.bindings_prop_lexical_value, 71,
           "bindings property must not be overwritten for lexical");
  assertEq(env.bindings_prop_unqualified, true,
           "bindings property must live in the with env for bindings");
  assertEq(env.bindings_prop_unqualified_value, 80,
           "bindings property must be overwritten for unqualified");

  env = envs[i]; i++;
  assertEq(env.type, "[DebugProxy] CallObject");
  assertEq(env.qualified, true, "qualified var must live in the CallObject");
  assertEq(env.unqualified, false);
  assertEq(env.lexical, false);
  assertEq(env.prop, false);
  assertEq(env.bindings_prop, false);

  assertEq(env.bindings_prop_var, true,
           "qualified var binding must be created in the CallObject even if it conflicts with the bindings object property");
  assertEq(env.bindings_prop_var_value, undefined,
           "qualified var value must not be set if it conflicts with the bindings object property");
  assertEq(env.bindings_prop_lexical, false);
  assertEq(env.bindings_prop_unqualified, false);

  env = envs[i]; i++;
  assertEq(env.type, "[DebugProxy] GlobalLexicalEnvironmentObject");
  assertEq(env.qualified, false);
  assertEq(env.unqualified, false);
  assertEq(env.lexical, false);
  assertEq(env.prop, false);
  assertEq(env.bindings_prop, false);

  assertEq(env.bindings_prop_var, false);
  assertEq(env.bindings_prop_lexical, false);
  assertEq(env.bindings_prop_unqualified, false);

  env = envs[i]; i++;
  assertEq(env.type, "*global*");
  assertEq(env.qualified, false);
  assertEq(env.unqualified, true, "unqualified name must live in the global");
  assertEq(env.lexical, false);
  assertEq(env.prop, true, "this property must live in the global");
  assertEq(env.bindings_prop, false);

  assertEq(env.bindings_prop_var, false);
  assertEq(env.bindings_prop_lexical, false);
  assertEq(env.bindings_prop_unqualified, false);
};

g.eval(`
function target() {
}
target();
`);
