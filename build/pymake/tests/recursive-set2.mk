#T returncode: 2

FOO = $(BAR)
BAR = $(FOO)

all:
	echo $(FOO)
	@echo TEST-FAIL
