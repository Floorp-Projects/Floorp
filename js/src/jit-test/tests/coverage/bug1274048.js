// |jit-test| --code-coverage

function h() {
    return 1;
}
function g() {
    switch (h()) {}
}
g();
getLcovInfo();
