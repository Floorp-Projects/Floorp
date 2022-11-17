/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Verify the environment chain for frame scripts described in
// js/src/vm/EnvironmentObject.h.

const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);

XPCShellContentUtils.init(this);

add_task(async function unique_scope() {
  const page = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });

  const envsPromise = new Promise(resolve => {
    Services.mm.addMessageListener("unique-envs-result", msg => {
      resolve(msg.data);
    });
  });
  const sharePromise = new Promise(resolve => {
    Services.mm.addMessageListener("unique-share-result", msg => {
      resolve(msg.data);
    });
  });

  const runInUniqueScope = true;
  const runInGlobalScope = !runInUniqueScope;

  Services.mm.loadFrameScript(`data:,
var unique_qualified = 10;
unique_unqualified = 20;
let unique_lexical = 30;
this.unique_prop = 40;

const funcs = Cu.getJSTestingFunctions();
const envs = [];
let env = funcs.getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: funcs.getEnvironmentObjectType(env) || "*BackstagePass*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "unique_qualified"),
    unqualified: !!Object.getOwnPropertyDescriptor(env, "unique_unqualified"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "unique_lexical"),
    prop: !!Object.getOwnPropertyDescriptor(env, "unique_prop"),
  });

  env = funcs.getEnclosingEnvironmentObject(env);
}

sendSyncMessage("unique-envs-result", envs);
`, false, runInGlobalScope);

  Services.mm.loadFrameScript(`data:,
sendSyncMessage("unique-share-result", {
  unique_qualified: typeof unique_qualified,
  unique_unqualified: typeof unique_unqualified,
  unique_lexical: typeof unique_lexical,
  unique_prop: this.unique_prop,
});
`, false, runInGlobalScope);

  const envs = await envsPromise;
  const share = await sharePromise;

  Assert.equal(envs.length, 5);

  let i = 0, env;

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, true, "lexical must live in the NSLEO");
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "WithEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, true, "this property must live in the with env");

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticVariablesObject");
  Assert.equal(env.qualified, true, "qualified var must live in the NSVO");
  Assert.equal(env.unqualified, true, "unqualified var must live in the NSVO");
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "GlobalLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "*BackstagePass*");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  Assert.equal(share.unique_qualified, "undefined", "qualified var must not be shared");
  Assert.equal(share.unique_unqualified, "undefined", "unqualified name must not be shared");
  Assert.equal(share.unique_lexical, "undefined", "lexical must not be shared");
  Assert.equal(share.unique_prop, 40, "this property must be shared");

  await page.close();
});

add_task(async function non_unique_scope() {
  const page = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });

  const envsPromise = new Promise(resolve => {
    Services.mm.addMessageListener("non-unique-envs-result", msg => {
      resolve(msg.data);
    });
  });
  const sharePromise = new Promise(resolve => {
    Services.mm.addMessageListener("non-unique-share-result", msg => {
      resolve(msg.data);
    });
  });

  const runInUniqueScope = false;
  const runInGlobalScope = !runInUniqueScope;

  Services.mm.loadFrameScript(`data:,
var non_unique_qualified = 10;
non_unique_unqualified = 20;
let non_unique_lexical = 30;
this.non_unique_prop = 40;

const funcs = Cu.getJSTestingFunctions();
const envs = [];
let env = funcs.getInnerMostEnvironmentObject();
while (env) {
  envs.push({
    type: funcs.getEnvironmentObjectType(env) || "*BackstagePass*",
    qualified: !!Object.getOwnPropertyDescriptor(env, "non_unique_qualified"),
    unqualified: !!Object.getOwnPropertyDescriptor(env, "non_unique_unqualified"),
    lexical: !!Object.getOwnPropertyDescriptor(env, "non_unique_lexical"),
    prop: !!Object.getOwnPropertyDescriptor(env, "non_unique_prop"),
  });

  env = funcs.getEnclosingEnvironmentObject(env);
}

sendSyncMessage("non-unique-envs-result", envs);
`, false, runInGlobalScope);

  Services.mm.loadFrameScript(`data:,
sendSyncMessage("non-unique-share-result", {
  non_unique_qualified,
  non_unique_unqualified,
  non_unique_lexical,
  non_unique_prop,
});
`, false, runInGlobalScope);

  const envs = await envsPromise;
  const share = await sharePromise;

  Assert.equal(envs.length, 4);

  let i = 0, env;

  env = envs[i]; i++;
  Assert.equal(env.type, "NonSyntacticLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, true, "lexical must live in the NSLEO");
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "WithEnvironmentObject");
  Assert.equal(env.qualified, true, "qualified var must live in the with env");
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, true, "this property must live in the with env");

  env = envs[i]; i++;
  Assert.equal(env.type, "GlobalLexicalEnvironmentObject");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, false);
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  env = envs[i]; i++;
  Assert.equal(env.type, "*BackstagePass*");
  Assert.equal(env.qualified, false);
  Assert.equal(env.unqualified, true, "unqualified name must live in the backstage pass");
  Assert.equal(env.lexical, false);
  Assert.equal(env.prop, false);

  Assert.equal(share.non_unique_qualified, 10, "qualified var must be shared");
  Assert.equal(share.non_unique_unqualified, 20, "unqualified name must be shared");
  Assert.equal(share.non_unique_lexical, 30, "lexical must be shared");
  Assert.equal(share.non_unique_prop, 40, "this property must be shared");

  await page.close();
});
