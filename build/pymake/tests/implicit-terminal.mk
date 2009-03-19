#T returncode: 2

# the %.object rule is "terminal". This means that additional implicit rules cannot be chained to it.

all: test.prog
	test "$$(cat $<)" = "Program: Object: Source: test.source"
	@echo TEST-FAIL

%.prog: %.object
	printf "Program: %s" "$$(cat $<)" > $@

%.object:: %.source
	printf "Object: %s" "$$(cat $<)" > $@

%.source:
	printf "Source: %s" $@ > $@
