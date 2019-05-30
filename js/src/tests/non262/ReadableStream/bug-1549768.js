var otherGlobal = newGlobal({newCompartment: true});
var obj = { start(c) {} };
var Cancel = otherGlobal.ReadableStream.prototype.tee.call(new ReadableStream(obj))[0].cancel;

var stream = new ReadableStream(obj);
var [branch1, branch2] = ReadableStream.prototype.tee.call(stream);

Cancel.call(branch1, {});

gczeal(2, 1);

Cancel.call(branch2, {});

if (typeof reportCompare === 'function') {
  reportCompare(0, 0);
}
