#T commandline: ['-j3']

# Commands of double-colon rules are always executed in order.

all: dc
	cat status
	test "$$(cat status)" = "all1:all2:"
	@echo TEST-PASS

dc:: slowt
	printf "all1:" >> status

dc::
	sleep 0.2
	printf "all2:" >> status

slowt:
	sleep 1
