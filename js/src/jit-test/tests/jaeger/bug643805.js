function _tt_face_get_name() {
    var __label__ = -1; 
    var $rec;
    var $n;
    while(true) {
        switch(__label__) {
        case -1:
            $rec=0;
            $n=0;
        case 0:
            if ($rec == 20) {
                __label__ = 2;
                break;
            }
            var $63 = $n;
            var $64 = $63 + 1;
            $n = $64;
            var $65 = $rec;
            $rec = $rec + 1;
            assertEq($64 < 30, true);
            __label__ = 0;
            break;
        case 1:
            $rec = 0;
        case 2:
            return;
        }
    }
}
_tt_face_get_name();

/* Test tracking of lifetimes around backedges in nested loops. */
function nested() {
  var x = 100;
  var i = 0;
  while (i < 10) {
    while (i < 10) {
      i++;
      if (x < 20)
        break;
      if (i > 10) {
        x = 200;
        i++;
      }
    }
    if (i > 10)
      x = 100;
  }
}
nested();
