#T returncode: 2

# The dependencies of the command rule of a single-colon target are resolved before the rules without commands.

all: export

export:
	sleep 1
	touch somefile

all: somefile
	test -f somefile
	@echo TEST-PASS
