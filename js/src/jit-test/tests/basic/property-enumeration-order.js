function check(obj, expected, expectedAll = expected) {
    assertEq(Reflect.ownKeys(obj).join(""), expectedAll);
    assertEq(Object.getOwnPropertyNames(obj).join(""), expectedAll);
    assertEq(Object.keys(obj).join(""), expected);
    var s = "";
    for (var x in obj)
	s += x;
    assertEq(s, expected);
}

var obj = {2: 0, 0: 0, "foo": "bar", 4: 0, "-5": 1};
check(obj, "024foo-5");
obj.x = 1;
check(obj, "024foo-5x");
obj[1] = 1;
check(obj, "0124foo-5x");
obj[1.2] = 1;
check(obj, "0124foo-5x1.2");
Object.defineProperty(obj, '3', {value:1,enumerable:false,configurable:false,writable:false});
check(obj, "0124foo-5x1.2", "01234foo-5x1.2");
delete obj[2];
check(obj, "014foo-5x1.2", "0134foo-5x1.2");
obj[2] = 1;
check(obj, "0124foo-5x1.2", "01234foo-5x1.2");

assertEq(JSON.stringify(obj), '{"0":0,"1":1,"2":1,"4":0,"foo":"bar","-5":1,"x":1,"1.2":1}');

var arr = [1, 2, 3];
Object.defineProperty(arr, '3', {value:1,enumerable:true,configurable:true,writable:true});
check(arr, "0123", "0123length");
Object.defineProperty(arr, '1000', {value:1,enumerable:true,configurable:true,writable:false});
check(arr, "01231000", "01231000length");

arr = [1, 2, , , 5];
check(arr, "014", "014length");
arr[0xfffffff] = 4;
check(arr, "014268435455", "014268435455length");
