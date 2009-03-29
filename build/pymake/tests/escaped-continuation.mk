#T returncode: 2

all:
	echo "Hello" \\
	test "world" = "not!"
	@echo TEST-PASS
