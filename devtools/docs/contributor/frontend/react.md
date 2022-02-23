
We use [React](http://facebook.github.io/react/) to write our user
interfaces. In here you can find an explanation of why we chose React
and a short primer on it. Additionally, we list best practices that
all devtools code should adhere to when writing React.

# Quick Intro

This is a very quick introduction on how to *use* React, but does not
explain in-depth the concepts behind it. If you want more in-depth
articles, I recommend the following links:

* http://facebook.github.io/react/docs/tutorial.html - the official tutorial
* https://github.com/petehunt/react-howto - how to learn React
* http://jlongster.com/Removing-User-Interface-Complexity,-or-Why-React-is-Awesome - long read but explains the concepts in depth

React embraces components as a way of thinking about UIs. Components
are the center of everything: they are composable like functions,
testable like JSON data, and provide lifecycle APIs for more complex
scenarios.

A component can represent anything from a single item in a list to a
complete virtualized grid that is made up of sub-components. They can
be used to abstract out "behaviors" instead of UI elements (think of a
`Selectable` component). React's API makes it easy to break up your UI
into whatever abstractions you need.

The core idea of a component is simple: it's something that takes
properties and returns a DOM-like structure.

```js
function Item({ name, iconURL }) {
  return div({ className: "item" },
             img({ className: "icon", href: iconURL }),
             name);
}
```

The `div` and `span` functions refer to `React.DOM.div` and
`React.DOM.span`. React provides constructors for all DOM elements on
`React.DOM`. These conform to the standard API for creating elements:
the first argument takes properties, and the rest are children.

You can see component composition kick in when using `Item`:

```js
const Item = React.createFactory(require('./Item'));

function List({ items }) {
  return div({ className: "list" },
             items.map(item => Item({ name: item.name, icon: item.iconURL)));
}
```

You can use custom components exactly the same way you use native
ones! The only difference is we wrapped it in a factory when importing
instead of using the React.DOM functions. Factories are just a way of
turning a component into a convenient function. Without factories, you
need to do do `React.createElement(Item, { ... })`, which is exactly
the same as `Item({ ... })` if using a factory.

## Rendering and Updating Components

Now that we have some components, how do we render them? You use
`React.render` for that:

```js
let items = [{ name: "Dubois", iconURL: "dubois.png" },
             { name: "Ivy", iconURL: "ivy.png" }];

React.render(List({ items: items }),
             document.querySelector("#mount"));
```

This renders a `List` component, given `items`, to a DOM node with an
id of `mount`. Typically you have a top-level `App` component that is
the root of everything, and you would render it like so.

What about updating? First, let's talk about data. The above
components take data from above and render out DOM structure. If any
user events were involved, the components would call callbacks passed
as props, so events walk back up the hierarchy. The conceptual model
is data goes down, and events come up.

You usually want to change data in response to events, and rerender
the UI with the new data. What does that look like? There are two
places where React will rerender components:

1\. Any additional `React.render` calls. Once a component is mounted,
you can call `React.render` again to the same place and React will see
that it's already mounted and perform an update instead of a full
render. For example, this code adds an item in response to an event
and updates the UI, and will perform optimal incremental updates:

```js
function addItem(item) {
  render([...items, item]);
}

function render(items) {
  React.render(List({ items: items,
                      onAddItem: addItem }),
               document.querySelector("#mount"));
}

render(items);
```

2\. Changing component local state. This is much more common. React
allows components to have local state, and whenever the state is
changed with the `setState` API it will rerender that specific
component. If you use component local state, you need to create a
component with `createClass`:

```js
const App = React.createClass({
  getInitialState: function() {
    return { items: [] };
  },

  handleAddItem: function(item) {
    const items = [...this.props.items, item];
    this.setState({ items: items });
  },

  render: function() {
    return List({ items: this.state.items,
                  onAddItem: this.handleAddItem });
  }
});
  ```

If you are using something like Redux to manage state this is handled
automatically for you with the library you use to bind Redux with
React. See more in [Redux](redux.html).

## DOM Diffing

What does it mean when React "updates" a component, and how does it
know which DOM to change? React achieves this with a technique called
DOM diffing. This alleviates the need for the programmer to worry
about how updates are actually applied to the DOM, and components can
render DOM structure declaratively in response to data. In the above
examples, when adding an item, React knows to only add a new DOM node
instead of recreating the whole list each time.

DOM diffing is possible because our components return what's called
"virtual DOM": a lightweight JSON structure that React can use to diff
against previous versions, and generate minimal changes to the real DOM.

This also makes it really east to test components with a real DOM:
just make sure the virtual DOM has what it should.

## Next

Read the [React Guidelines](react-guidelines.md) next to learn how to
write React code specifically for the devtools.