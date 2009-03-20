all: test.prog
	test "$$(cat $<)" = "Program: Object: Source: test.source"
	@echo TEST-PASS

%.prog: %.object
	printf "Program: %s" "$$(cat $<)" > $@

%.object: %.source
	printf "Object: %s" "$$(cat $<)" > $@

%.source:
	printf "Source: %s" $@ > $@
