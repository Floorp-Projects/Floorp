var qualified = 10;
// NOTE: JSM cannot have unqualified name.
let lexical = 30;
this.prop = 40;

const funcs = Cu.getJSTestingFunctions();
const envs = [];
let env = funcs.getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: funcs.getEnvironmentObjectType(env) || "*BackstagePass*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "qualified"),
    prop: !!Object.getOwnPropertyDescriptor(env, "prop"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "lexical"),
  });

  env = funcs.getEnclosingEnvironmentObject(env);
}

const EXPORTED_SYMBOLS = ["envs"];
