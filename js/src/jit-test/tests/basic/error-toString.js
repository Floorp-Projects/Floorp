var errorToString = Error.prototype.toString;


assertEq(errorToString.call({message: "", name: ""}), "Error");
assertEq(errorToString.call({name: undefined, message: ""}), "Error");
assertEq(errorToString.call({name: "Test", message: undefined}), "Test");
assertEq(errorToString.call({name: "Test", message: ""}), "Test");
assertEq(errorToString.call({name: "", message: "Test"}), "Test");
assertEq(errorToString.call({name: "Test", message: "it!"}), "Test: it!");
