VAR = value1
VAR := value2

VAR2 := val1
VAR2 = val2

default:
	test "$(flavor VAR)" = "simple"
	test "$(VAR)" = "value2"
	test "$(flavor VAR2)" = "recursive"
	test "$(VAR2)" = "val2"
	@echo "TEST-PASS"
