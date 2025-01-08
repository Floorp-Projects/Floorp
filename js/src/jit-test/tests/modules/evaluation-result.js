async function parseAndEvaluate(source, filename, kind) {
    let m = parseModule(source, filename, kind);
    moduleLink(m);
    return await moduleEvaluate(m);
}

(async () => {
  assertEq(await parseAndEvaluate("[]", "", "js"), undefined);
  assertEq(await parseAndEvaluate("[]", "", "json"), undefined);
})();
drainJobQueue();
