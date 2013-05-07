.DEFAULT_GOAL := default

not-default:
	@echo TEST-FAIL did not run default rule

default:
	@echo TEST-PASS
