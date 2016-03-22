
# Guidelines for Writing React

These are soft rules for writing react devtools code. Try to stick to
these for consistency, and if you disagree, file a bug to change these
docs and we can talk about it.

**Please also read** the [coding
 standards](https://wiki.mozilla.org/DevTools/CodingStandards#React_.26_Redux)
for react and redux code. The guidelines here are more general
patterns not specific to code style.

### Why no JSX?

You probably already noticed we don't use JSX. The answer isn't
complicated: we don't build our JS code, and we write directly for our
JS engine, SpiderMonkey. It already supports much of ES6, but it does
not support JSX (which is not a standard).

This may change if we ever adopt a build step. Even so, the author is
not convinced that JSX helps enough to warrant all the syntax. It is
clearer sometimes, but it can be noisy switching between JSX and JS a
lot.

It's not as bad as you may think! If you are used to JSX it may be an
adjustment, but you won't miss it too much.

### One component per file

Try to only put one component in a file. This helps avoid large files
full of components, but it's also technically required for how we wrap
components with factories. See the next rule.

It also makes it easier to write tests because you might not export
some components, so tests can't access them.

You can include small helper components in the same file if you really
want to, but note that they won't be directly tested and you will have
to use `React.createElement` or immediately wrap them in factories to
use them.

### Export the component directly and create factory on import

Modules are the way components interact. Ideally every component lives
in a separate file and they require whatever they need. This allows
tests to access all components and use module boundaries to wrap
components.

For example, we don't use JSX, so we need to create factories for
components to use them as functions. A simple way to do this is on
import:

```js
const Thing1 = React.createFactory(require('./thing1'));
const Thing2 = React.createFactory(require('./thing2'));
```

It adds a little noise, but then you can do `Thing1({ ... })` instead
of `React.createElement(Thing1, { ... })`. Definitely worth it.

Additionally, make sure to export the component class directly:

```js
const Thing1 = React.createClass({ ... });
module.exports = Thing1;
```

Do not export `{ Thing1 }` or anything like that. This is required for
the factory wrapping as well as hot reloading.

### More to Come

This is just a start. We will add more to this document.