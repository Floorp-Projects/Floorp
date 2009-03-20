test.foo: %.foo:
	test "$@" = "test.foo"
	@echo TEST-PASS made test.foo by default

all:
	@echo TEST-FAIL made $@, should have made test.foo
