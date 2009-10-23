#T gmake skip

$(shell \
touch filemissing; \
sleep 2; \
touch file1; \
)

all: file1
	@echo TEST-PASS

includedeps $(TESTPATH)/includedeps.deps

file1:
	@echo TEST-FAIL
