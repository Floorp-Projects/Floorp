assertEq(["a"].map(escape)[0], ["a"].map(function(s) {return escape(s);})[0]);
