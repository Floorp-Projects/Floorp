### Getting data from the store

To get data from the store, use `connect()`.

When using connect, you'll break up your component into two parts:

1. The part that displays the data (presentational component)

        // todos.js
        const Todos = React.createClass({
          propTypes: {
            todos: PropTypes.array.isRequired
          }

          render: function() {...}
        })

        module.exports = Todos;

2. The part that gets the data from the store (container component)

        // todos-container.js
        const Todos = require("path/to/todos");

        function mapStateToProps(state) {
          return {
            todos: state.todos
          };
        }

        module.exports = connect(mapStateToProps)(Todos);


`connect()` generates the container component. It wraps around the presentational component that was passed in (e.g. Todos).

The `mapStateToProps` is often called a selector. That's because it selects data from the state object. When the container component is rendering, the the selector will be called. It will pick out the data that the presentational component is going to need. Redux will take this object and pass it in to the presentational component as props.

With this setup, a presentational component is easy to share across different apps. It doesn't have any dependencies on the app, or any hardcoded expectations about how to get data. It just gets props that are passed to it and renders them.

For more advanced use cases, you can pass additional parameters into the selector and `connect()` functions. Read about those in the [`connect()`](https://github.com/reactjs/react-redux/blob/master/docs/api.md#connectmapstatetoprops-mapdispatchtoprops-mergeprops-options) docs.

---

Need to answer the following questions:

* How do I do I load asynchronous data?
* How do I do optimistic updates or respond to errors from async work?
* Do I use Immutable.js for my state?
* What file structure should I use?
* How do I test redux code?

And more.
