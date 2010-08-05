var obj = {firstAttr: 'value', secondAttr: 'another value', thirdAttr: 'the last value'};

(function() {
    for (var i = 0; i < 64; ++i) {
        var name;
        switch (~~(i / 4) % 3) {
            case 0: name = 'firstAttr'; break;
            case 1: name = 'secondAttr'; break;
            case 2: name = 'thirdAttr'; break;
        }

        var result = obj[name];

        switch (name) {
          case 'firstAttr': assertEq(result, 'value'); break;
          case 'secondAttr': assertEq(result, 'another value'); break;
          case 'thirdAttr': assertEq(result, 'the last value'); break;
        }
    }
})();

/* Rotate lookup between three ids. */
