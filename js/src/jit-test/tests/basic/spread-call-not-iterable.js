load(libdir + "asserts.js");

assertThrowsInstanceOf(() => Math.sin(...true), TypeError);
assertThrowsInstanceOf(() => Math.sin(...false), TypeError);
assertThrowsInstanceOf(() => Math.sin(...new Date()), TypeError);
assertThrowsInstanceOf(() => Math.sin(...Function("")), TypeError);
assertThrowsInstanceOf(() => Math.sin(...function () {}), TypeError);
assertThrowsInstanceOf(() => Math.sin(...(x => x)), TypeError);
assertThrowsInstanceOf(() => Math.sin(...1), TypeError);
assertThrowsInstanceOf(() => Math.sin(...{}), TypeError);
assertThrowsInstanceOf(() => Math.sin(...{ iterator: 10 }), TypeError);
assertThrowsInstanceOf(() => Math.sin(...{ iterator: function() undefined }), TypeError);
assertThrowsInstanceOf(() => Math.sin(...{ iterator: function() this }), TypeError);
assertThrowsValue(() => Math.sin(...{ iterator: function() this, next: function() { throw 10; } }), 10);
assertThrowsInstanceOf(() => Math.sin(.../a/), TypeError);
assertThrowsInstanceOf(() => Math.sin(...new Error()), TypeError);
