// |jit-test| error: x is not defined
setObjectMetadataCallback(function(obj) {
    var res = {};
    return res;
  });
gczeal(4);
x();
