# we should succeed in making foo.ooo from foo.ooo.test
all: foo.ooo
	@echo TEST-PASS

%.ooo: %.ccc
	exit 1

# this match-anything rule is terminal, and therefore applies
%:: %.test
	cp $< $@

foo.ooo.test:
	touch $@
