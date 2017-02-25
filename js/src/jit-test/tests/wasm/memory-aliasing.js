var i = wasmEvalText(
`(module
   (memory 1) (data (i32.const 0) "\\01\\02\\03\\04\\05\\06\\07\\08")
   (func $off1 (param $base i32) (result i32)
     (i32.add
       (i32.load8_u (get_local $base))
       (i32.load8_u offset=1 (get_local $base)))
   )
   (export "off1" $off1)
   (func $off2 (param $base i32) (result i32)
     (i32.add
       (i32.load8_u offset=1 (get_local $base))
       (i32.load8_u offset=2 (get_local $base)))
   )
   (export "off2" $off2)
)`).exports;
assertEq(i.off1(0), 3);
assertEq(i.off1(1), 5);
assertEq(i.off1(2), 7);
assertEq(i.off1(3), 9);
assertEq(i.off2(0), 5);
assertEq(i.off2(1), 7);
assertEq(i.off2(2), 9);
assertEq(i.off2(3), 11);
