# JavaScript coding standards

Probably the best piece of advice is **to be consistent with the rest of the code in the file**.

We use [ESLint](http://eslint.org/) to analyse JavaScript files automatically, either from within a code editor or from the command line. Here's [our guide to install and configure it](./eslint.md).

For quick reference, here are some of the main code style rules:

* file references to browser globals such as `window` and `document` need `/* eslint-env browser */` at the top of the file,
* lines should be 90 characters maximum,
* indent with 2 spaces (no tabs!),
* `camelCasePlease`,
* don't open braces on the next line,
* don't name function expressions: `let o = { doSomething: function doSomething() {} };`,
* use a space before opening paren for anonymous functions, but don't use one for named functions:
  * anonymous functions: `function () {}`
  * named functions: `function foo() {}`
  * anonymous generators: `function* () {}`
  * named generators: `function* foo() {}`
* aim for short functions, 24 lines max (ESLint has a rule that checks for function complexity too),
* `aArguments aAre the aDevil` (don't use them please),
* `"use strict";` globally per module,
* `semicolons; // use them`,
* no comma-first,
* consider using Task.jsm (`Task.async`, `Task.spawn`) for nice-looking asynchronous code instead of formatting endless `.then` chains,<!--TODO: shouldn't we advise async/await now?-->
* use ES6 syntax:
  * `function setBreakpoint({url, line, column}) { ... }`,
  * `(...args) => { }` rest args are awesome, no need for `arguments`,
  * `for..of` loops,
* don't use non-standard SpiderMonkey-only syntax:
  * no `for each` loops,
  * no `function () implicitReturnVal`,
  * getters / setters require { },
* only import specific, explicitly-declared symbols into your namespace:
  * `const { foo, bar } = require("foo/bar");`,
  * `const { foo, bar } = Cu.import("...", {});`,
* use Maps, Sets, WeakMaps when possible,
* use [template strings](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Template_literals) whenever possible to avoid concatenation, allow multi-line strings, and interpolation.


## Comments

Commenting code is important, but bad comments can hurt too, so it's important to have a few rules in mind when commenting:

* If the code can be rewritten to be made more readable, then that should be preferred over writing an explanation as a comment.
* Instead of writing a comment to explain the meaning of a poorly chosen variable name, then rename that variable.
* Avoid long separator comments like `// ****************** another section below **********`. They are often a sign that you should split a file in multiple files.
* Line comments go above the code they are commenting, not at the end of the line.
* Sentences in comments start with a capital letter and end with a period.
* Watch out for typos.
* Obsolete copy/pasted code hurts, make sure you update comments inside copy/pasted code blocks.
* A global comment at the very top of a file explaining what the file is about and the major types/classes/functions it contains is a good idea for quickly browsing code.
* If you are forced to employ some kind of hack in your code, and there's no way around it, then add a comment that explains the hack and why it is needed. The reviewer is going to ask for one anyway.
* Bullet points in comments should use stars aligned with the first comment to format each point
```javascript
// headline comment
// * bullet point 1
// * bullet point 2
```

## Asynchronous code

A lot of code in DevTools is asynchronous, because a lot of it relies on connecting to the DevTools debugger server and getting information from there in an asynchronous fashion.

It's easy to make mistakes with asynchronous code, so here are a few guidelines that should help:

* Prefer [promises](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise) over callbacks.
* Use the `new Promise(() => {})` syntax.
* Don't forget to catch rejections by defining a rejection handler: `promise.then(() => console.log("resolved"), () => console.log("rejected"));` or `promise.catch(() => console.log("rejected"));`.
* Make use of [`Tasks` and generator functions](https://developer.mozilla.org/en-US/docs/Mozilla/JavaScript_code_modules/Task.jsm) to make asynchronous code look synchronous. <!--TODO async/await as in the TODO above?-->

## React & Redux

There are React-specific code style rules in the .eslintrc file.

### Components

* Default to creating components as [stateless function components](https://facebook.github.io/react/docs/reusable-components.html#stateless-functions). 
* If you need local state or lifecycle methods, use `React.createClass` instead of functions.
* Use React.DOM to create native elements. Assign it to a variable named `dom`, and use it like `dom.div({}, dom.span({}))`. You may also destructure specific elements directly: `const { div, ul } = React.DOM`.

### PropTypes

* Use [PropTypes](https://facebook.github.io/react/docs/reusable-components.html#prop-validation) to define the expected properties of your component. Each directly accessed property (or child of a property) should have a corresponding PropType.
* Use `isRequired` for any required properties.
* Place the propTypes definition at the top of the component. If using a stateless function component, place it above the declaration of the function.
* Where the children property is used, consider [validating the children](http://www.mattzabriskie.com/blog/react-validating-children).


