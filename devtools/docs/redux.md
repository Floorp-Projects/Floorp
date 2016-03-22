
We use [Redux](https://github.com/reactjs/redux) to manage application
state. The [docs](http://redux.js.org/) do a good job explaining the
concepts, so go read them.

# Quick Intro

Just like the [React introduction](react.html), this is a quick
introduction to redux, focusing on how it fits into React and why we
chose it.

One of the core problems that React does not address is managing
state. In the React intro, we talked about data flowing down and
events flowing up. Conceptually this is nice, but you quickly run into
awkward situations in large apps.

Let's look at an example. Say you have a page with a tabbed interface.
Here, `Tab1` is managing a list of items, so naturally it uses local
state. `Tab2` renders different stuff.

```js
const Tab1 = React.createClass({
  getInitialState: function() {
    return { items: [] };
  },

  handleAddItem: function(item) {
    this.setState({ items: [...this.state.items, item]});
  },

  render: function() {
    /* ... Renders the items and button to add new item ... */
  }
});

const Tab2 = React.createClass({
  render: function() {
    /* ... Renders other data ... */
  }
});

// Assume `Tab1` and `Tab2` are wrapped with a factory when importing
const Tabs = React.createClass({
  render: function() {
    return div(
      { className: 'tabs' },
      // ... Render the tab buttons ...
      Tab1(),
      Tab2()
    );
  }
});
```

What happens when `Tab2` needs the list of items though? This scenario
comes up all time: components that aren't directly related need access
to the same state. A small change would be to move the `items` state
up to the `Tabs` component, and pass it down to both `Tab1` and `Tab2`.

But now `Tabs` has to implement the `handleAddItem` method to add an
item because it's managing that state. This quickly gets ugly as the
end result is the root component ends up with a ton of state and
methods to manage it: a [god
component](https://en.wikipedia.org/wiki/God_object) is born.

Additionally, how do we know what data each tab needs? We end up
passing *all* the state down because we don't know. This is not a
modular solution: one object managing the state and every component
receiving the entire state is like using tons of global variables.

## Evolution of Flux

Facebook addressed this with the
[flux](https://facebook.github.io/flux/) architecture, which takes the
state out of the components and into a "store". Redux is the latest
evolution of this idea and solves a lot of problems previous flux
libraries had (read it's documentation for more info).

Because the state exists outside the component tree, any component can
read from it. Additionally, **state is updated with
[actions](http://redux.js.org/docs/basics/Actions.html)** that any
component can fire. We have [guidelines](redux-guidelines) for where
to read/write state, but it completely solves the problem described
above. Both `Tab1` and `Tab2` would be listening for changes in the
`item` state, and `Tab1` would fire actions to change it.

With redux, **state is managed modularly with
[reducers](http://redux.js.org/docs/basics/Reducers.html)** but tied
together into a single object. This means a single JS object
represents most* of your state. It may sound crazy at first, but think
of it as an object with references to many pieces of state; that's all
it is.

This makes it very easy to test, debug, and generally think about. You
can log your entire state to the console and inspect it. You can even
dump in old states and "replay" to see how the UI changed over time.

I said "most*" because it's perfectly fine to use both component local
state and redux. Be aware that any debugging tools will not see local
state at all though. It should only be used for transient state; we'll
talk more about that in the guidelines.

## Immutability

Another important concept is immutability. In large apps, mutating
state makes it very hard to track what changed when. It's very easy to
run into situations where something changes out from under you, and
the UI is rendered with invalid data.

Redux enforces the state to be updated immutably. That means you
always return new state. It doesn't mean you do a deep copy of the
state each time: when you need to change some part of the tree you
only need to create new objects to replace the ones your changing (and
walk up to the root to create a new root). Unchanged subtrees will
reference the same objects.

This removes a whole class of errors, almost like Rust removing a
whole class of memory errors by enforcing ownership.

## Order of Execution

One of best things about React is that **rendering is synchronous**. That
means when you render a component, given some data, it will fully
render in the same tick. If you want the UI to change over time, you
have to change the *data* and rerender, instead of arbitrary UI
mutations.

The reason this is desired is because if you build the UI around
promises or event emitters, updating the UI becomes very brittle
because anything can happen at any time. The state might be updated in
the middle of rendering it, maybe because you resolved a few promises
which made your rendering code run a few ticks later.

Redux embraces the synchronous execution semantics as well. What this
means is that everything happens in a very controlled way. When
updating state through an action, all reducers are run and a new state
is synchronously generated. At that point, the new state is handed off
to React and synchronously rendered.

Updating and rendering happen in two phases, so the UI will *always*
represent consistent state. The state can never be in the middle of
updating when rendering.

What about asynchronous work? That's where
[middleware](http://redux.js.org/docs/advanced/Middleware.html) come
in. At this point you should probably go study our code, but
middleware allows you to dispatch special actions that indicate
asynchronous work. The middleware will catch these actions and do
something async, dispatching "raw" actions along the way (it's common
to emit a START, DONE, and ERROR action).

**Ultimately there are 3 "phases" or level of abstraction**: the async
layer talks to the network and may dispatch actions, actions are
synchronously pumped through reducers to generate state, and state is
rendered with react.

## Next

Read the [Redux Guidelines](redux-guidelines.md) next to learn how to
write React code specifically for the devtools.