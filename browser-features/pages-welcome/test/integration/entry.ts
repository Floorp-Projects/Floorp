import { setup } from "./bridge.ts";
import { faults, prefs } from "../../../../libs/ui/test/page-fixture.ts";
setup();
const parameters = new URLSearchParams(location.search);
if (parameters.has("releaseNotes")) {
  prefs.set("floorp.releaseNotes.mode", "support");
  prefs.set("floorp.releaseNotes.choiceConfirmed", parameters.has("confirmed"));
  if (parameters.has("readFailure")) faults.read = "floorp.releaseNotes.mode";
}
await import("../../src/main.tsx");
const { runPageTests } = parameters.has("releaseNotes")
  ? await import("./release-notes-prompt.test.ts")
  : await import("./welcome.test.ts");
await runPageTests();
