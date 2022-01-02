// |jit-test| allow-oom; allow-unhandlable-oom

gcparam("maxBytes", gcparam("gcBytes") + 4*1024);
arr = [1e0, 5e1, 9e19, 0.1e20, 1.3e20, 1e20, 9e20, 9.99e20, 
    0.1e21, 1e21, 1e21+65537, 1e21+65536, 1e21-65536, 1]; 
for (var i = 0; i < 4000; i++) {
    arr.push(1e19 + i*1e19);
}
for (var i in arr) {}
