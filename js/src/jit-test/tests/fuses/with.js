let iterProto = [].values().__proto__
with (newGlobal()) {
    const v3 = [].values();
    Object.defineProperty(v3.__proto__, "return", {});
    const v18 = [];
    for (let i = 0; i < 500; i++) {
        [] = v18;
    }
}
