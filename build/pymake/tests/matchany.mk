#T returncode: 2

# we should fail to make foo.ooo from foo.ooo.test
all: foo.ooo
	@echo TEST-FAIL

%.ooo:

# this match-anything pattern should not apply to %.ooo
%: %.test
	cp $< $@

foo.ooo.test:
	touch $@
