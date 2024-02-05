# TypeScript Experiment

This folder contains an experiment to add TypeScript to Gecko. The type checking can be run manually via:

```
cd devtools/client/performance-new
yarn install
yarn test
```

Also, the types should work with editor integration. VS Code works with TypeScript by default, and should pick up the types here.

The type checking is also included in the DevTools node tests, which can be run manually via:
```
node devtools/client/bin/devtools-node-test-runner.js --suite=performance
```

More importantly the DevTools node tests run on Continuous Integration. They are included in the DevTools presets `devtools` and `devtools-linux`. They can also be found via `mach try fuzzy`, under the name "source-test-node-devtools-tests". To recap, the following try pushes will run the DevTools node tests:

DevTools node tests are also automatically run for any Phabricator diff which impacts DevTools. If the job fails, a bot will add a comment on the corresponding Phabricator diff.

## Do not overload require

Anytime that our code creates the `require` function through a BrowserLoader, it can conflict with the TypeScript type system. For example:

```
const { require } = BrowserLoader(...);
```

TypeScript treats `require` as a special keyword. If the variable is defined on the page, then it shadow's TypeScript's keyword, and the require machinery will be improperly typed as an `any`. Care needs to be taken to get around this. Here is a solution of hiding the `require` function from TypeScript:

```
const browserLoader = BrowserLoader(...);

/** @type {any} - */
const scope = this;
scope.require = browserLoader.require;
```
