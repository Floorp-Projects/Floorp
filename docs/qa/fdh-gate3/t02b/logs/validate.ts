import { validateManifest } from "/tmp/fdh-gate2/pr2b2/tools/qa3/evidence/validate.ts";
for (const p of ["/tmp/fdh-gate3/acceptance/t02b/evidence-gate3-t02b.json", "/tmp/fdh-gate3/acceptance/t02b/evidence-gate3-t02b-postdown.json"]) {
  const m = JSON.parse(await Deno.readTextFile(p));
  console.log(p.split("/").pop(), "errors:", JSON.stringify(validateManifest(m, { role: "runner" })));
}
