// |jit-test| ion-eager; allow-oom
x = [];
x[12] = 1;
x.unshift(0);
x.unshift(0);
x.sort(function() {
  oomAfterAllocations(5);
})
