function selfsetelem(o, i) {
    o[i] = o;
}
var arr = new Array();
selfsetelem(arr, "prop0");
selfsetelem(arr, 0);
selfsetelem(arr, 1);
selfsetelem(arr, 0);
arr.prop0.toString();
