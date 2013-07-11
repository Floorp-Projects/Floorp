# When a target is defined multiple times, the prerequisites should get
# merged.

default: foo bar baz

foo:
	test "$<" = "foo.in1"
	@echo TEST-PASS

foo: foo.in1

bar: bar.in1
	test "$<" = "bar.in1"
	test "$^" = "bar.in1 bar.in2"
	@echo TEST-PASS

bar: bar.in2

baz: baz.in2
baz: baz.in1
	test "$<" = "baz.in1"
	test "$^" = "baz.in1 baz.in2"
	@echo TEST-PASS

foo.in1 bar.in1 bar.in2 baz.in1 baz.in2:
