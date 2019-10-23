# TypeScript Experiment

This folder contains an experiment to add TypeScript to Gecko. The type checking is not integrated into continuous integration as of yet, and can be run manually via:

```
cd devtools/client/performance-new
yarn install
yarn test
```

Also, the types should work with editor integration. VS Code works with TypeScript by default, and should pick up the types here.

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
