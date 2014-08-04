a = {}
b = __proto__
for (i = 0; i < 10; i++) {
    __proto__ &=  a
    a.__proto__ = b
}
