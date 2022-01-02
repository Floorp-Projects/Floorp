// |reftest| module

async function f() {
    return "success";
}

var AsyncFunction = (async function(){}).constructor;

assertEq(f instanceof AsyncFunction, true);

f().then(v => {
    reportCompare("success", v);
});
