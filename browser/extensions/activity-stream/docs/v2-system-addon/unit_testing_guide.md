# Unit testing in Activity Stream

## Overview

Our unit tests in Activity Stream are written with mocha, chai, and sinon, and run
with karma. They include unit tests for both content code (React components, etc.)
and `.jsm`s.

You can find unit tests in `system-addon/tests/unit`.

## Running tests

To run the unit tests once, run `npm run testmc`.

To run unit tests continuously (i.e. in "test-driven development" mode), you can
run `npm run tddmc`.

## Debugging tests

To debug tests, you should run them in continous mode with `npm run tddmc`. In the
Firefox window that is opened (it should say "Karma... - connected"), click the
"debug" button and open your console to see test output, set breakpoints, etc.

Unfortunately, source maps for tests do not currently work in Firefox. If you need
to see line numbers, you can run the tests with Chrome by running
`npm run tddmc -- --browsers Chrome`

## Where to put new tests

If you are creating a new test, add it to a subdirectory of the `system-addon/tests/unit`
that corresponds to the file you are testing. Tests should end with `.test.js` or
`.test.jsx` if the test includes any jsx.

For example, if the file you are testing is `system-addon/lib/Foo.jsm`, the test
file should be `system-addon/test/unit/lib/Foo.test.js`

## Mocha tests

All our unit tests are written with [mocha](https://mochajs.org), which injects
globals like `describe`, `it`, `beforeEach`, and others. It can be used to write
synchronous or asynchronous tests:

```js
describe("FooModule", () => {
  // A synchronous test
  it("should create an instance", () => {
    assert.instanceOf(new FooModule(), FooModule);
  });
  describe("#meaningOfLife", () => {
    // An asynchronous test
    it("should eventually get the meaning of life", async () => {
      const foo = new FooModule();
      const result = await foo.meaningOfLife();
      assert.equal(result, 42);
    });
  });
});
```

## Assertions

To write assertions, use the globally available `assert` object (this is provided
by karma-chai, so you do not need to `require` it).

For example:

```js
assert.equal(foo, 3);
assert.propertyVal(someObj, "foo", 3);
assert.calledOnce(someStub);
```

You can use any of the assertions from:

- [`chai`](http://chaijs.com/api/assert/).
- [`sinon-chai`](https://github.com/domenic/sinon-chai#assertions)

### Custom assertions

We have some custom assertions for checking various types of actions:

#### `.isUserEventAction(action)`

Asserts that a given `action` is a valid User Event, i.e. that it contains only
expected/valid properties for User Events in Activity Stream.

```js
// This will pass
assert.isUserEventAction(ac.UserEvent({event: "CLICK"}));

// This will fail
assert.isUserEventAction({type: "FOO"});

// This will fail because BLOOP is not a valid event type
assert.isUserEventAction(ac.UserEvent({event: "BLOOP"}));
```

## Overriding globals in `.jsm`s

Most `.jsm`s you will be testing use `Cu.import` or `XPCOMUtils` to inject globals.
In order to add mocks/stubs/fakes for these globals, you should use the `GlobalOverrider`
utility in `system-addon/test/unit/utils`:

```js
const {GlobalOverrider} = require("test/unit/utils");
describe("MyModule", () => {
  let globals;
  let sandbox;
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox; // this is a sinon sandbox
    // This will inject a "AboutNewTab" global before each test
    globals.set("AboutNewTab", {override: sandbox.stub()});
  });
  // globals.restore() clears any globals you added as well as the sinon sandbox
  afterEach(() => globals.restore());
});
```

## Testing React components

You should use the [enzyme](https://github.com/airbnb/enzyme) suite of test utilities
to test React Components for Activity Stream.

Where possible, use the [shallow rendering method](https://github.com/airbnb/enzyme/blob/master/docs/api/shallow.md) (this will avoid unnecessarily
rendering child components):

```js
const React = require("react");
const {shallow} = require("enzyme");

describe("<Foo>", () => {
  it("should be hidden by default", () => {
    const wrapper = shallow(<Foo />);
    assert.isTrue(wrapper.find(".wrapper").props().hidden);
  });
});
```

If you need to, you can also do [Full DOM rendering](https://github.com/airbnb/enzyme/blob/master/docs/api/mount.md)
with enzyme's `mount` utility.

```js
const React = require("react");
const {mount} = require("enzyme");
...
const wrapper = mount(<Foo />);
```

### Rendering localized components

If you need to render a component that contains localized components, use the
`shallowWithIntl` and `mountWithIntl` functions in `system-addon/test/unit/utils`.
