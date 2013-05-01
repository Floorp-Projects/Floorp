assertEq((function() {
  'use asm';
  function _main() {
    var $1=0, $2=0, $3=0, $4=0, $5=0, $6=0, $7=0, $8=0, $9=0, $10=0, label=0;
    label = 1;
    while (1) {
      switch (label | 0) {
       case 1:
        $2 = $1 + 14 | 0;
        $3 = $1;
        label = 20;
        break;
       case 20:
        $5 = $2;
        $4 = $3;
        label = 24;
        break;
       case 24:
        $7 = $5 + 1 | 0;
        $8 = $4 + 1 | 0;
        return $8|0;
       case 49:
        $9 = $6 + 1 | 0;
        if ($10) {
          $6 = $9;
          break;
        }
        return 0;
      }
    }
    return 0;
  }
  return _main;
})()(), 1);
