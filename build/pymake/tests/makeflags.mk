#T environment: {'MAKEFLAGS': 'OVAR=oval'}

all:
	test "$(OVAR)" = "oval"
	@echo TEST-PASS

