#T returncode: 2

# Required include targets that fail should abort execution.

include dummy.mk

all:
	@echo TEST-FAIL

dummy.mk:
	@echo "Evaluated dummy.mk"
	exit 1
