function thunk() {
    new.target();
}
assertThrownErrorContains(thunk, "new.target");

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
