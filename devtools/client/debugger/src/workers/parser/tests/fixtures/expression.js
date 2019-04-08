const obj = { a: { b: 2 } }; // e.g. obj.a.b
const foo = obj2.c.secondProperty; // e.g. foo.obj2.c.secondProperty

// computed properties
const com = { [a]: { b: "c", [d]: "e" }, [b]: 3 }; // e.g. com[a].b
const firstAuthor = collection.books[1].author;
const firstActionDirector = collection.genres["sci-fi"].movies[0].director;

app.TodoView = Backbone.extend({
  render: function() {}
});

// assignments
obj.foo = { a: { b: "c" }, b: 3 }; // e.g. obj.foo.a.b
com = { a: { b: "c" }, b: 3 }; // e.g. com.a.b

// arrays
const res = [{ a: 2 }, { b: 3 }]; // e.g. res[1].b
const res2 = { a: [{ b: 2 }] }; // e.g. res.a[0].b
const res3 = { a: [{ b: 2 }], b: [{ c: 3 }] }; // e.g. res.a[0].b
const res4 = [[{ a: 3 }], [{ b: a.b.c.v.d }]]; // e.g. res[1][0].b

function params({ a, b }) {} // e.g. a
var pars = function({ a, b }) {};
const evil = obj2.doEvil().c.secondProperty; // e.g. obj2.doEvil or ""
