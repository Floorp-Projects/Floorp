import { validateManifest } from "/tmp/fdh-gate2/pr2b2/tools/qa3/evidence/validate.ts";
const value = JSON.parse(Deno.readTextFileSync("/tmp/fdh-gate4/acceptance/4a-agent1/evidence-postdown.json"));
const errors = validateManifest(value, { role: "runner" });
console.log(JSON.stringify({ role: "runner", errorCount: errors.length, errors }, null, 2));
