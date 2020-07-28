// |jit-test| skip-if: !('oomTest' in this)
var code = `
  (\`\${key}: \${args[1]?.toString()}\`)
`;
oomTest(function() { return parseModule(code); });
