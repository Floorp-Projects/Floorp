import { validateManifest } from "/tmp/fdh-gate2/pr2b2/tools/qa3/evidence/validate.ts";
const value = JSON.parse(await Deno.readTextFile("/tmp/fdh-gate2/pr2b2/_dist/fdh/evidence/gate3-t02.json"));
const errors = validateManifest(value, { role: "runner" });
console.log("validateManifest errors:", JSON.stringify(errors));
Deno.exit(errors.length === 0 ? 0 : 1);
