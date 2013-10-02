# This test ensures that a local variable in a $(foreach) is bound to
# the local value, not a global value.
i := dummy

all:
	test "$(foreach i,foo bar,found:$(i))" = "found:foo found:bar"
	test "$(i)" = "dummy"
	@echo TEST-PASS
