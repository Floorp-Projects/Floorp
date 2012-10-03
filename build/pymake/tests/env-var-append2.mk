#T environment: {'FOO': '$(BAZ)'}

FOO += $(BAR)
BAR := PASS
BAZ := TEST

all:
	@echo $(subst $(NULL) ,-,$(FOO))
