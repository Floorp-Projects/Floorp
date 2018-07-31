## Sourcemapped test fixtures

The test fixtures in this folder are used to generate a suite of tests for sourcemapped
files that have been passed through various tools like Babel and Typescript, and various
bundlers like Rollup and Webpack.

The fixtures follow a naming scheme where:

* `babel-` means the file requires Babel in order to be consumed by a bundler.
* `typescript-` means the file requires Typescript in order to be consumed by a bundler.
* `-cjs` means the file is either:
  * Written in CommonJS
  * May be converted to CommonJS when Babel is enabled

  This is used to exclude these files from processing by bundlers that expect input
  files to be ES modules.
