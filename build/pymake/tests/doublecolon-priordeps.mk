#T commandline: ['-j3']

# All *prior* dependencies of a doublecolon rule must be satisfied before
# subsequent commands are run.

all:: target1

all:: target2
	test -f target1
	@echo TEST-PASS

target1:
	touch starting-$@
	sleep 1
	touch $@

target2:
	sleep 0.1
	test -f starting-target1
