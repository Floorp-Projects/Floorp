/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  // Disable handling of //# source(Mapping)URL= comments.
  await SpecialPowers.pushPrefEnv({
    set: [["javascript.options.source_pragmas", false]]
  });

  const dbg = await initDebugger("doc-source-pragma.html");

  // The sourceURL pragma didn't rename the source
  await waitForSource(dbg, "source-pragma.js");
  const source = findSource(dbg, "source-pragma.js");
  const actors = dbg.selectors.getSourceActorsForSource(source.id);

  is(actors.length, 1, "have a single actor");
  ok(actors[0].url.endsWith("/source-pragma.js"), "source url was not rewritten");
  is(actors[0].sourceMapURL, null, "source map was not exposed");
});
