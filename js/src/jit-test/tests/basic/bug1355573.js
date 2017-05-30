// |jit-test| error:overflow
if (getBuildConfiguration().debug === true)
    throw "overflow";
function f(){};
Object.defineProperty(f, "name", {value: "a".repeat((1<<28)-1)});
len = f.bind().name.length;
