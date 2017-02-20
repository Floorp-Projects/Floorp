if (helperThreadCount() === 0)
  quit();
setInterruptCallback(function() { print("MainThread Interrupt!"); cooperativeYield(); return true; });
evalInCooperativeThread('\
  setInterruptCallback(function() { print("Worker Interrupt!"); cooperativeYield(); return true; });\
  for (var i = 0; i < 10; i++) {\
     print("Worker: " + i);\
     interruptIf(true);\
  }\
  print("Worker Done!");\
');
for (var i = 0; i < 10; i++) {
    print("MainThread: " + i);
    interruptIf(true);
}
print("MainThread Done!");
