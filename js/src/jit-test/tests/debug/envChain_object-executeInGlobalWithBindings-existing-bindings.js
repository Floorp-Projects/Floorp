// Verify the environment chain for Debugger.Object described in
// js/src/vm/EnvironmentObject.h.

// WithEnvironmentObject shouldn't contain bindings conflicts with existing
// globals.

const g = newGlobal({newCompartment: true});
const dbg = new Debugger();
const gw = dbg.addDebuggee(g);

const bindings = {
  bindings_prop: 50,

  bindings_prop_var: 61,
  bindings_prop_lexical: 71,
  bindings_prop_unqualified: 81,
};

gw.executeInGlobal(`
var bindings_prop_var = 60;
let bindings_prop_lexical = 70;
bindings_prop_unqualified = 80;
`);

const {envs, vars} = JSON.parse(gw.executeInGlobalWithBindings(`
const vars = {
  bindings_prop,
  bindings_prop_var,
  bindings_prop_lexical,
  bindings_prop_unqualified,
};

const envs = [];
let env = getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: getEnvironmentObjectType(env) || "*global*",
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

assertEq(vars.bindings_prop, 50,
         "qualified var should read the value set by the declaration");
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

assertEq(envs.length, 3);

let i = 0, env;

env = envs[i]; i++;
assertEq(env.type, "WithEnvironmentObject");
assertEq(env.bindings_prop, true, "bindings property must live in the with env for bindings");

assertEq(env.bindings_prop_var, false,
         "bindings property must not live in the with env for bindings if it conflicts with existing global");
assertEq(env.bindings_prop_lexical, false,
         "bindings property must not live in the with env for bindings if it conflicts with existing global");
assertEq(env.bindings_prop_unqualified, false,
         "bindings property must not live in the with env for bindings if it conflicts with existing global");
assertEq(env.bindings_prop_unqualified, false,
         "bindings property must not live in the with env for bindings if it conflicts with existing global");

env = envs[i]; i++;
assertEq(env.type, "GlobalLexicalEnvironmentObject");
assertEq(env.bindings_prop, false);

assertEq(env.bindings_prop_var, false);
assertEq(env.bindings_prop_lexical, true);
assertEq(env.bindings_prop_lexical_value, 70);
assertEq(env.bindings_prop_unqualified, false);

env = envs[i]; i++;
assertEq(env.type, "*global*");
assertEq(env.bindings_prop, false);

assertEq(env.bindings_prop_var, true);
assertEq(env.bindings_prop_var_value, 60);
assertEq(env.bindings_prop_lexical, false);
assertEq(env.bindings_prop_unqualified, true);
assertEq(env.bindings_prop_unqualified_value, 80);
