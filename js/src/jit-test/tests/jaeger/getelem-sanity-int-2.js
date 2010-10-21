var arr = ['this', 'works', 'for', 'me'];
for (var i = 0; i < arr.length; ++i) {
    var result = arr[i];
    switch (i) {
      case 0: assertEq('this', result); break;
      case 1: assertEq('works', result); break;
      case 2: assertEq('for', result); break;
      case 3: assertEq('me', result); break;
    }
}

/* int32 getelem for dense array. */
