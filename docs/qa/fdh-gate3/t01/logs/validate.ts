import { validateManifest } from "/tmp/fdh-gate2/pr2b2/tools/qa3/evidence/validate.ts";
const p = "/tmp/fdh-gate3/acceptance/t01-final/evidence-gate3-t01-final.json";
const m = JSON.parse(await Deno.readTextFile(p));
console.log("validate errors:", JSON.stringify(validateManifest(m, { role: "runner" })));
