function f() {
    return (42.0 + Math.abs(1.0e60)) | 0;
}
assertEq(f(), 0);
assertEq(f(), 0);
