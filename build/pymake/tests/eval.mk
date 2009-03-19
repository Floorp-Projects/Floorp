TESTVAR = val1

$(eval TESTVAR = val2)

all:
	test "$(TESTVAR)" = "val2"
	@echo TEST-PASS
