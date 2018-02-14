const __v_16876 = 50000;
const __v_16877 = '"use asm";\nfunction f() { LOCALS }\nreturn f;';
for (var __v_16878 = __v_16876; __v_16878 < __v_16876 + 2; ++__v_16878) {
  const __v_16879 = __v_16877.replace('LOCALS', Array(__v_16878).fill().map((__v_16882, __v_16883) => 'var l' + __v_16883 + ' = 0;').join('\n'));
  const __v_16880 = new Function(__v_16879);
}