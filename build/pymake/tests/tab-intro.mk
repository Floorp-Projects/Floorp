# Initial tab characters should be treated well.

	THIS = a value

	ifdef THIS
	VAR = conditional value
	endif

all:
	test "$(THIS)" = "another value"
	test "$(VAR)" = "conditional value"
	@echo TEST-PASS

THAT = makefile syntax

	THIS = another value
