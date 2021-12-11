var BUGNUMBER = {
  valueOf: function() {
    ++undefined;
  }
};
BUGNUMBER + 1;
