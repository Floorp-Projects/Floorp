#T returncode: 2

ifeq ($(FOO,VAR))
all:
	@echo TEST_FAIL
endif
