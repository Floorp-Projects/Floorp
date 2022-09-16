const target = {};
Services.scriptloader.loadSubScript(`data:,
var qualified = 10;
unqualified = 20;
let lexical = 30;
this.prop = 40;

const funcs = Cu.getJSTestingFunctions();
const envs = [];
let env = funcs.getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: funcs.getEnvironmentObjectType(env) || "*BackstagePass*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "qualified"),
    unqualified: !!Object.getOwnPropertyDescriptor(env, "unqualified"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "lexical"),
    prop: !!Object.getOwnPropertyDescriptor(env, "prop"),
  });

  env = funcs.getEnclosingEnvironmentObject(env);
}

this.ENVS = envs;
`, target);

const envs = target.ENVS;
const EXPORTED_SYMBOLS = ["envs"];
