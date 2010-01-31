actual = '';
expected = "undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,1,1,1,1,1,2,2,2,2,2,3,3,3,3,3,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,"; 

var index;

function h() {
  for (var i = 0; i < 5; ++i) {
    var a = arguments;
    appendToActual(a[index]);
  }
}

index = 0;
h();
index = -1;
h();
index = 1;
h();

index = -9999999;
h(1, 2, 3);
index = -1;
h(1, 2, 3);
index = 0;
h(1, 2, 3);
index = 1;
h(1, 2, 3);
index = 2;
h(1, 2, 3);
index = 3;
h(1, 2, 3);
index = 4;
h(1, 2, 3);
index = 9999999;
h(1, 2, 3);

assertEq(actual, expected)
