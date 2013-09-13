#T gmake skip

FILE = includedeps-variables

all: $(FILE)1

includedeps $(TESTPATH)/includedeps-variables.deps

filemissing:
	@echo TEST-PASS
