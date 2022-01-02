function f0(p0) {
  var v0 = 0.5;
  var v1 = 1.5;
  var v2 = 2.5;
  var v3 = 3.5;
  var v4 = 4.5;
  var v5 = 5.5;
  var v6 = 6.5;
  var v7 = 7.5;
  var v8 = 8.5;
  var v9 = 9.5;
  var v10 = 10.5;
  var v11 = 11.5;
  var v12 = 12.5;
  var v13 = 13.5;
  var v14 = 14.5;
  var v15 = 15.5;
  var v16 = 16.5;
  // 0.125 is used to avoid the oracle choice for int32.
  while (0) {
   // p0 = false;
    var tmp = v0;
    v0 = 0.125 + v0 + v1;
    v1 = 0.125 + v1 + v2;
    v2 = 0.125 + v2 + v3;
    v3 = 0.125 + v3 + v4;
    v4 = 0.125 + v4 + v5;
    v5 = 0.125 + v5 + v6;
    v6 = 0.125 + v6 + v7;
    v7 = 0.125 + v7 + v8;
    v8 = 0.125 + v8 + v9;
    v9 = 0.125 + v9 + v10;
    v10 = 0.125 + v10 + v11;
    v11 = 0.125 + v11 + v12;
    v12 = 0.125 + v12 + v13;
    v13 = 0.125 + v13 + v14;
    v14 = 0.125 + v14 + v15;
    v15 = 0.125 + v15 + v16;
    v16 = 0.125 + v16 + tmp;
  }
  return 0.5 + v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + v16;
}

// expect 145
assertEq(f0(false), 145);

