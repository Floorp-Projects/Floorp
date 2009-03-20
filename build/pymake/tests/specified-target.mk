#T commandline: ['VAR=all', '$(VAR)']

all:
	@echo TEST-FAIL: unexpected target 'all'

$$(VAR):
	@echo TEST-PASS: expected target '$$(VAR)'
