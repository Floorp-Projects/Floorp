#T environment: {'FOO': 'TEST'}

FOO += $(BAR)
BAR := PASS

all:
	@echo $(subst $(NULL) ,-,$(FOO))
