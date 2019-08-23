# DevTools node tests

In addition to mochitests and xpcshell tests, some panels in DevTools are using node test libraries to run unit tests. For instance, several panels are using [Jest](https://jestjs.io/) to run React component unit tests.

## Find the node tests on Try

The DevTools node tests are split in two different test suites on try:
- `node(devtools)`: all the DevTools node tests, except the ones for the debugger
- `node(debugger)`: only the Debugger node tests

They are running on the `Linux 64 opt` platform. They are both tier 1 jobs, which means that any failure will lead to a backout.

## Run Tests On Try

To run the DevTools node tests on try, you can use `./mach try fuzzy` and look for the jobs named `source-test-node-debugger-tests` and `source-test-node-devtools-tests`.

They are also run when using the "devtools" preset: `./mach try --preset devtools`.

### Node tests try job definition

The definition of those try jobs can be found at [taskcluster/ci/source-test/node.yml](https://searchfox.org/mozilla-central/source/taskcluster/ci/source-test/node.yml).

The definition also contains the list of files that will trigger the node test jobs. Currently the debugger tests run when any file is modified under `devtools/client/debugger`, the devtools tests run when any file is modified under `devtools/client` or `devtools/shared`.

## Run Tests Locally

### Prerequisite: yarn

You will need yarn to be installed in order to run both the debugger and the DevTools tests. See https://yarnpkg.com/docs/install/.

### Debugger

To run the debugger node tests:
```
> cd devtools/client/debugger/
> yarn && node bin/try-runner.js
```

Note that the debugger is running other tasks than just unit tests: `flow`, `eslint`, `stylelint` etc...
Using `yarn && yarn test` would only run the Jest tests, while `node bin/try-runner` will run the same tests and scripts as the ones used on try.

### DevTools

To run the other (non-debugger) DevTools tests, the easiest is to rely on the same script as the one used to run the tests on try:
```
> node devtools/client/bin/devtools-node-test-runner.js --suite={suitename}
```

At the moment of writing, the supported suites for this script are: `aboutdebugging-new`, `accessibility`, `application`, `framework`, `netmonitor`, `webconsole`.

Alternatively, you can also locate the `package.json` corresponding to a given suite, and run `yarn && yarn test`.

## Updating snapshots

Some of the node tests are snapshot tests, which means they compare the output of a given component to a previous text snapshot. They might break if you are legitimately modifying a component and it means the snapshots need to be updated.

A snapshot failure will show up as follows:
```
› 1 snapshot failed from 1 test suite
```

It should also mention the command you can run to update the snapshots:
```
Inspect your code changes or run `yarn run test-ci -u` to update them.
```

For example, if you need to update snapshots in a specific panel, first locate the package.json corresponding to the node test folder of the panel. In theory it should be under `devtools/client/{panelname}/test/node/` but it might be slightly different depending on each panel. Then run `yarn run test-ci -u` in this folder and add the snapshot changes to your commit.
