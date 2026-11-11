(async () => {
  const mod = ChromeUtils.importESModule("chrome://remote/content/marionette/actors/MarionetteCommandsParent.sys.mjs");
  const bc = gBrowser.selectedBrowser.browsingContext;
  const actor = mod.getMarionetteCommandsActorProxy(() => bc);
  const raw = await actor.executeScript("return JSON.stringify({url: location.href, hit: document.body.innerText.includes(\"QA3-T01-MARKER\")})", [], {});
  const parsed = JSON.parse(raw);
  return parsed.hit ? "QA3-T01-MARKER @ " + parsed.url : "ABSENT @ " + parsed.url;
})()
