async function* f() {
    return "success";
}

var AsyncGenerator = (async function*(){}).constructor;

assertEq(f instanceof AsyncGenerator, true);

f().next().then(v => {
    reportCompare("success", v.value);
});
