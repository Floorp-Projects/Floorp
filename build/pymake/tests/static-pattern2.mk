all: foo.out
	test -f $^
	@echo TEST-PASS

foo.out: %.out: %.in
	test "$*" = "foo"
	cp $^ $@

foo.in:
	touch $@
