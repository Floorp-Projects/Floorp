function f(code) {
    eval(code)
}
f("\
    function h({x}) {\
        print(x)\
    }\
    h(/x/);\
")
