const fs = require("fs");

module.exports = {
  ...JSON.parse(fs.readFileSync(__dirname + "/../../../.prettierrc")),
  overrides: [
    {
      files: [
        "src/**/*.js",
        "packages/*/src/**/*.js",
      ],
      options: {
        // The debugger uses Babel 7 and some newer Flow features.
        // Unfortunately, Prettier has not yet adopted a version of Babel's
        // parser with this fix: https://github.com/babel/babel/pull/9891
        // That necessitates us to override to config to explicitly tell
        // Prettier that our files contain Flowtype annotations.
        "parser": "babel-flow"
      },
    },
  ],
};
