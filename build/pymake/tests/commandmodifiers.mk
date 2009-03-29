define COMMAND
$(1)
 	$(1)

endef

all:
	$(call COMMAND,@true #TEST-FAIL)
	$(call COMMAND,-exit 4)
	$(call COMMAND,@-exit 1 # TEST-FAIL)
	$(call COMMAND,-@exit 1 # TEST-FAIL)
	$(call COMMAND,+exit 0)
	$(call COMMAND,+-exit 1)
	$(call COMMAND,@+exit 0 # TEST-FAIL)
	$(call COMMAND,+@exit 0 # TEST-FAIL)
	$(call COMMAND,-+@exit 1 # TEST-FAIL)
	$(call COMMAND,+-@exit 1 # TEST-FAIL)
	$(call COMMAND,@+-exit 1 # TEST-FAIL)
	$(call COMMAND,@+-@+-exit 1 # TEST-FAIL)
	$(call COMMAND,@@++exit 0 # TEST-FAIL)
	@echo TEST-PASS
