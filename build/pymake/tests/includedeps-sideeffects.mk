#T gmake skip
#T returncode: 2

all: file1 filemissing
	@echo TEST-PASS

includedeps $(TESTPATH)/includedeps.deps

file:
	touch $@
