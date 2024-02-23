var code = `
  (\`\${key}: \${(args[1]?.toString)?.()}\`)
`;
oomTest(function() { return parseModule(code); });
