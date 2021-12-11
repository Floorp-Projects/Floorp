// |jit-test| error:ReferenceError

(function() {
    var $10=0;
    while (1) {
      switch (stack.label & 2) {
       case 1:
        return $8|0;
       case 49:
        if ($10) {}
    }
  }
})()()
