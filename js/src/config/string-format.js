String.prototype.format = function string_format() {
  // there are two modes of operation... unnamed indices are read in order;
  // named indices using %(name)s. The two styles cannot be mixed.
  // Unnamed indices can be passed as either a single argument to this function,
  // multiple arguments to this function, or as a single array argument
  let curindex = 0;
  let d;

  if (arguments.length > 1) {
    d = arguments;
  }
  else
    d = arguments[0];

  function r(s, key, type) {
    if (type == '%')
      return '%';
    
    let v;
    if (key == "") {
      if (curindex == -1)
        throw Error("Cannot mix named and positional indices in string formatting.");

      if (curindex == 0 && (!(d instanceof Object) || !(0 in d))) {
        v = d;
      }
      else if (!(curindex in d))
        throw Error("Insufficient number of items in format, requesting item %i".format(curindex));
      else {
        v = d[curindex];
      }

      ++curindex;
    }
    else {
      key = key.slice(1, -1);
      if (curindex > 0)
        throw Error("Cannot mix named and positional indices in string formatting.");
      curindex = -1;

      if (!(key in d))
        throw Error("Key '%s' not present during string substitution.".format(key));
      v = d[key];
    }
    switch (type) {
    case "s":
      if (v === undefined)
        return "<undefined>";
      return v.toString();
    case "r":
      return uneval(v);
    case "i":
      return parseInt(v);
    case "f":
      return Number(v);
    default:
      throw Error("Unexpected format character '%s'.".format(type));
    }
  }
  return this.replace(/%(\([^)]+\))?(.)/g, r);
};
