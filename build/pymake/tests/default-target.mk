test: VAR = value

%.do:
	@echo TEST-FAIL: ran target "$@", should have run "all"

.PHONY: test

all:
	@echo TEST-PASS: the default target is all

test:
	@echo TEST-FAIL: ran target "$@", should have run "all"

test.do:
