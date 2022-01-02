const foo = function() {};

const bar = () => {};

const TodoView = Backbone.View.extend({
  tagName: "li",
  initialize: function() {},
  doThing(b) {
    console.log("hi", b);
  },
  render: function() {
    return this;
  }
});
