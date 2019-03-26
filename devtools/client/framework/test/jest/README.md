# Jest Tests for devtools/client/framework

## About

DevTools React components can be tested using [jest](https://jestjs.io/). Jest allows to test our UI components in isolation and complement our end to end mochitests.

## Run locally

We use yarn for dependency management. To run the tests locally:
```
  cd devtools/client/shared/framework/test/jest
  yarn && yarn test
```

## Run on try

The tests run on try on linux64 platforms. The complete name of the try job is `devtools-tests`. In treeherder, they will show up as `node(devtools)`.

Adding the tests to a try push depends on the try selector you are using.
- try fuzzy: look for the job named `source-test-node-devtools-tests`

The configuration file for try can be found at `taskcluster/ci/source-test/node.yml`
