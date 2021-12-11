function basic() {
    var zero = "0";
    var one  = "1";
    var thousand = String(1000);
    var max = String(0xffff);

    assertEq(zero, "0");
    assertEq(Number(zero), 0);
    assertEq(String(Number(zero)), "0");

    assertEq(one, "1");
    assertEq(Number(one), 1);
    assertEq(String(Number(one)), "1");

    assertEq(thousand, "1000");
    assertEq(Number(thousand), 1000);
    assertEq(String(Number(thousand)), "1000");

    assertEq(max, "65535");
    assertEq(Number(max), 0xffff);
    assertEq(String(Number(max)), "65535");
}

function index() {
    var zero = "0";
    var trippleZero = "000";

    var seven = "7";
    var doubleOhSeven = "007";

    var object = {0: "a", "000": "b"};
    var object2 = {7: "a", "007": "b"};

    var array = ["a"];
    array[trippleZero] = "b";
    var array2 = [0, 1, 2, 3, 4, 5, 6, "a"];
    array2[doubleOhSeven] = "b";

    for (var i = 0; i < 30; i++) {
        assertEq(object[zero], "a");
        assertEq(object[0], "a");
        assertEq(object[trippleZero], "b");

        assertEq(object2[seven], "a");
        assertEq(object2[7], "a");
        assertEq(object2[doubleOhSeven], "b");

        assertEq(array[zero], "a");
        assertEq(array[0], "a");
        assertEq(array[trippleZero], "b");

        assertEq(array2[seven], "a");
        assertEq(array2[7], "a");
        assertEq(array2[doubleOhSeven], "b");
    }
}

function forin() {
    var array = [0, 1, 2, 3, 4, 5, 6];

    var i = 0;
    for (var name in array) {
        assertEq(name, String(i));
        assertEq(Number(name), i);

        assertEq(array[name], i);
        assertEq(array.hasOwnProperty(name), true);

        i++;
    }
}

function parse() {
    var numbers = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0xffff];

    for (var number of numbers) {
        assertEq(parseInt(String(number)), number);
        assertEq(parseInt(String(number), 10), number);
        assertEq(parseInt(String(number), 0), number);
    }
}

basic();
index();
forin();
parse();
