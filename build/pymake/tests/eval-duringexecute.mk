#T returncode: 2

# Once parsing is finished, recursive expansion in commands are not allowed to create any new rules (it may only set variables)

define MORERULE
all:
	@echo TEST-FAIL
endef

all:
	$(eval $(MORERULE))
	@echo done
