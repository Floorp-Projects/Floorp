$(shell \
touch dep; \
sleep 2; \
touch all; \
)

all:: dep
	@echo TEST-PASS

.PHONY: all
