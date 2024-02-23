oomTest(new Function(`
  let kJSEmbeddingMaxTypes = 1000000;
  let kJSEmbeddingMaxFunctions = 1000000;
  let kJSEmbeddingMaxImports = 100000;
  const known_failures = {};
  function test(func, description) {
    known_failures[description]
  }
  function testLimit(name, min, limit, gen) {
    test(() => {}, \`Validate \${name} mininum\`);
    test(() => {}, \`Async compile \${name} over limit\`);
  }
  testLimit("types", 1, kJSEmbeddingMaxTypes, (builder, count) => {});
  testLimit("functions", 1, kJSEmbeddingMaxFunctions, (builder, count) => {});
  testLimit("imports", 1, kJSEmbeddingMaxImports, (builder, count) => {});
`));
