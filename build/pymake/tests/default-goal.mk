not-default:
	@echo TEST-FAIL did not run default rule

default:
	@echo $(if $(filter not-default,$(INTERMEDIATE_DEFAULT_GOAL)),TEST-PASS,TEST-FAIL .DEFAULT_GOAL not set by $(MAKE))

INTERMEDIATE_DEFAULT_GOAL := $(.DEFAULT_GOAL)
.DEFAULT_GOAL := default
