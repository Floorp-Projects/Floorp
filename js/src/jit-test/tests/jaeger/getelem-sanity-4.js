var obj = {
    attr0: 'val0',
    attr1: 'val1',
    attr2: 'val2',
    attr3: 'val3',
    attr4: 'val4',
    attr5: 'val5',
    attr6: 'val6',
    attr7: 'val7',
    attr8: 'val8',
    attr9: 'val9',
    attr10: 'val10',
    attr11: 'val11',
    attr12: 'val12',
    attr13: 'val13',
    attr14: 'val14',
    attr15: 'val15',
    attr16: 'val16',
    attr17: 'val17',
}

var baseName = 'attr';

(function() {
    for (var i = 0; i < 128; ++i) {
        var name = baseName + (i % 18);
        var result = obj[name];
        switch (i) {
          case 0: assertEq('val0', result); break;
          case 1: assertEq('val1', result); break;
          case 2: assertEq('val2', result); break;
          case 3: assertEq('val3', result); break;
          case 4: assertEq('val4', result); break;
          case 5: assertEq('val5', result); break;
          case 6: assertEq('val6', result); break;
          case 7: assertEq('val7', result); break;
          case 8: assertEq('val8', result); break;
          case 9: assertEq('val9', result); break;
          case 10: assertEq('val10', result); break;
          case 11: assertEq('val11', result); break;
          case 12: assertEq('val12', result); break;
          case 13: assertEq('val13', result); break;
          case 14: assertEq('val14', result); break;
          case 15: assertEq('val15', result); break;
          case 16: assertEq('val16', result); break;
          case 17: assertEq('val17', result); break;
        }
    }
})();

/* Megamorphic index atom. */
