var a = [1, 2];
a[3.1415926535] = 'value';

for (var i = 0; i < 3; i++) {
    var attr;
    switch (i) {
      case 0: attr = 0; break;
      case 1: attr = 1; break;
      case 2: attr = 3.1415926535; break;
    }
    var result = a[attr];
    switch (i) {
      case 0: assertEq(result, 1); break;
      case 1: assertEq(result, 2); break;
      case 2: assertEq(result, 'value'); break;
    }
}

/* int32_t and string getelem for non-dense array. */
