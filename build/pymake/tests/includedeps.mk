#T gmake skip

all: file1
	@echo TEST-PASS

includedeps $(TESTPATH)/includedeps.deps

file1:
	touch $@
