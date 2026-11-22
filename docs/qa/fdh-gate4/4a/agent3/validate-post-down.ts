import { validateManifest } from "file:///tmp/fdh-gate4/pr2b200/tools/qa3/evidence/validate.ts";

const manifestPath = "/tmp/fdh-gate4/acceptance/4a-agent3/evidence-post-down.json";
const manifest = JSON.parse(Deno.readTextFileSync(manifestPath));
const errors = validateManifest(manifest, { role: "runner" });
console.log(JSON.stringify({ errorCount: errors.length, errors }, null, 2));
if (errors.length !== 0) Deno.exit(1);
