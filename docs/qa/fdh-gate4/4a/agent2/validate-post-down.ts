import { validateManifest } from "file:///tmp/fdh-gate2/pr2b100/tools/qa3/evidence/validate.ts";

const manifestPath = "/tmp/fdh-gate4/acceptance/4a-agent2/evidence-post-down.json";
const manifest = JSON.parse(await Deno.readTextFile(manifestPath));
const errors = validateManifest(manifest, { role: "runner" });
const detail = errors.map((e) => ({ code: e.code, path: e.path, message: e.message }));
console.log(JSON.stringify({ errorCount: errors.length, errors: detail }, null, 2));
console.log(`state=${manifest.state} assertions=${Array.isArray(manifest.assertions) ? manifest.assertions.length : "n/a"} limitations=${Array.isArray(manifest.limitations) ? manifest.limitations.length : "n/a"}`);
if (errors.length > 0) Deno.exit(1);
