$(shell touch foo.in)

all: foo.out
	test "$(wildcard ./*.in)" = "./foo.in"
	@echo TEST-PASS

./%.out: %.in
	test "$@" = "foo.out"
	cp $< $@
