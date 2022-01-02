function foo() {
    const b = "".match();
    try {
	foo();
    } catch {}
}
foo()
