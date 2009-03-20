VAR = value$
VAR2 = other

all:
	test "$(VAR)" = "value"
	@echo TEST-PASS
