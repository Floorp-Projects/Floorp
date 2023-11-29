// |jit-test| --ion-offthread-compile=off 
// |jit-test| --fast-warmup 

this.__defineSetter__('sum', () => {})
sum=0;

options = {fileName: "test.js"}
evaluate("\
  function inline1(i)  { return i+0; }\
  function inline2(i)  { return i+1; }\
  function inline3(z,i){ return i+5; }\
  function a() {\
    for (let i=0; i<2000; i++) {\
      sum=inline1(i);\
      sum=inline2(i);\
      sum=inline3(sum,i);\
    }\
  }\
", options);
a();

evaluate("\
  function inline4(i)  { return i+0; }\
  function inline5(i)  { return i+1; }\
  function inline6(z,i){ return i+5; }\
  function b() {\
    for (let i=0; i<2000; i++) {\
      sum=sum+i;\
    }\
  }\
", options);
b();
