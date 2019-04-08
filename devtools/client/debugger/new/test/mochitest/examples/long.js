var app = {};

// Generic "model" object. You can use whatever
// framework you want. For this application it
// may not even be worth separating this logic
// out, but we do this to demonstrate one way to
// separate out parts of your application.
app.TodoModel = function (key) {
  this.key = key;
  this.todos = [];
  this.onChanges = [];
};

app.TodoModel.prototype.addTodo = function (title) {
  this.todos = this.todos.concat([{
	id: Utils.uuid(),
	title: title,
	completed: false
  }]);
};

app.TodoModel.prototype.inform = function() {
  // Something changed, but we do nothing
  return null;
};

app.TodoModel.prototype.toggleAll = function (checked) {
  // Note: it's usually better to use immutable data structures since they're
  // easier to reason about and React works very well with them. That's why
  // we use map() and filter() everywhere instead of mutating the array or
  // todo items themselves.
  this.todos = this.todos.map(function (todo) {
	return Object.assign({}, todo, {completed: checked});
  });

  this.inform();
};

app.TodoModel.prototype.toggle = function (todoToToggle) {
  this.todos = this.todos.map(function (todo) {
	return todo !== todoToToggle ?
	  todo :
	  Object.assign({}, todo, {completed: !todo.completed});
  });

  this.inform();
};

app.TodoModel.prototype.destroy = function (todo) {
  this.todos = this.todos.filter(function (candidate) {
	return candidate !== todo;
  });

  this.inform();
};

app.TodoModel.prototype.save = function (todoToSave, text) {
  this.todos = this.todos.map(function (todo) {
	return todo !== todoToSave ? todo : Object.assign({}, todo, {title: text});
  });

  this.inform();
};

app.TodoModel.prototype.clearCompleted = function () {
  this.todos = this.todos.filter(function (todo) {
	return !todo.completed;
  });

  this.inform();
};

function testModel() {
  const model = new app.TodoModel();
  model.clearCompleted();
}
