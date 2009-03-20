ifdef RANDOM
ifeq (,$(error Not evaluated!))
endif
endif

ifdef RANDOM
ifeq (,)
else ifeq (,$(error Not evaluated!))
endif
endif

all:
	@echo TEST-PASS
