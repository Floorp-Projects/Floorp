function t() {
    var o = {l: 0xfffffffff};
    var l = o.l - 0xffffffffe;
    var a = getSelfHostedValue('NewDenseArray');
    var arr = a(l);
    assertEq(arr.length, 1);
}
t();
t();
